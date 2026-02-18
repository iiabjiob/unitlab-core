// src/stores/channelStore.ts
import { defineStore } from "pinia"
import { ref } from "vue"

import { ChannelsAPI } from "@/api/channels.api"
import { DevicesAPI } from "@/api/devices.api"
import { getLogger } from "@/utils/logger"
import { normalizeChannel, ensureChannel, formatAoValue } from "@/utils/channel"

import { useDeviceStore } from "@/stores/deviceStore"
import { useWebSocketStore } from "@/stores/websocketStore"

import { useChannelLogStore } from "@/stores/channelLogStore"
import type { ChannelLogEntry } from "@/stores/channelLogStore"

import {
  WSAction,
  CmdMode,
  ReqStateMode,
  type SetDoCommandMessage,
  type SetAoCommandMessage,
  type RequestStateMessage,
} from "@/types/ws/messages"

import {
  StateMode,
  type DeviceStateEvent,
  type DeviceRespEvent,
} from "@/types/ws/events"

import { CHANNEL_TYPES, type Channel, type ChannelDto, type ChannelListDto, type DiChannel, type DoChannel, type DoChannelUiState, type TimeoutHandle } from "@/types/channel"
import {
  applyDeltaState,
  applyDiDiagnostics,
  applyDoDiagnostics,
  describeDiDiagnosticUpdates,
  type DiDiagnosticsBitmasks,
  type DiDiagnosticsChange,
  type DoDiagnosticsBitmasks,
} from "@/stores/utils/diagnostics"

const logger = getLogger("CHANNEL")

const COMMAND_PENDING_DEBOUNCE_MS = 150
const COMMAND_TIMEOUT_MS = 2000
const COMMAND_FAILURE_DISPLAY_MS = 2000

export const useChannelStore = defineStore("channelStore", () => {
  const channels = ref<Channel[]>([])
  const responses = ref<Record<string, DeviceRespEvent>>({})
  const stateRevision = ref(0)
  const lastStateChangedKeys = ref<Set<string>>(new Set())

  const isLoading = ref(false)
  const isLoaded = ref(false)
  const deviceLoading = new Set<number>()
  const deviceLoaded = new Set<number>()

  const logStore = useChannelLogStore()

  const actionCounters = new Map<number, number>()
  const actionQueues = new Map<number, string[]>()
  const aoActionMap = new Map<string, string>()
  const doStateRefreshTimers = new Map<number, TimeoutHandle>()
  const deviceLastStateAt = new Map<number, number>()
  const channelsIndexByDevice = new Map<number, Channel[]>()
  const channelsIndexByDeviceAndChannel = new Map<string, Channel>()
  const EMPTY_CHANNELS: readonly Channel[] = []

  function enqueueAction(deviceId: number): string {
    const next = (actionCounters.get(deviceId) ?? 0) + 1
    actionCounters.set(deviceId, next)
    const actionId = next.toString().padStart(4, "0")
    const queue = actionQueues.get(deviceId) ?? []
    queue.push(actionId)
    actionQueues.set(deviceId, queue)
    return actionId
  }

  function removeAction(deviceId: number, actionId?: string) {
    if (!actionId) return
    const queue = actionQueues.get(deviceId)
    if (!queue) return
    const idx = queue.indexOf(actionId)
    if (idx === -1) return
    queue.splice(idx, 1)
    if (!queue.length) {
      actionQueues.delete(deviceId)
    }
  }

  function aoKey(deviceId: number, chIndex: number) {
    return `${deviceId}:${chIndex}`
  }

  function channelKey(deviceId: number, chIndex: number) {
    return `${deviceId}:${chIndex}`
  }

  function rebuildChannelIndexes() {
    channelsIndexByDevice.clear()
    channelsIndexByDeviceAndChannel.clear()
    for (const channel of channels.value) {
      const byDevice = channelsIndexByDevice.get(channel.device_id)
      if (byDevice) {
        byDevice.push(channel)
      } else {
        channelsIndexByDevice.set(channel.device_id, [channel])
      }
      channelsIndexByDeviceAndChannel.set(channelKey(channel.device_id, channel.index), channel)
    }
  }

  function channelsByDeviceFast(deviceId: number): readonly Channel[] {
    return channelsIndexByDevice.get(deviceId) ?? EMPTY_CHANNELS
  }

  function channelByDeviceAndIndex(deviceId: number, chIndex: number): Channel | undefined {
    return channelsIndexByDeviceAndChannel.get(channelKey(deviceId, chIndex))
  }

  function registerAoAction(deviceId: number, chIndex: number, actionId: string) {
    aoActionMap.set(aoKey(deviceId, chIndex), actionId)
  }

  function peekAoAction(deviceId: number, chIndex: number): string | undefined {
    return aoActionMap.get(aoKey(deviceId, chIndex))
  }

  function clearAoAction(deviceId: number, chIndex: number) {
    aoActionMap.delete(aoKey(deviceId, chIndex))
  }

  const DIGITAL_ON = "ON" as const
  const DIGITAL_OFF = "OFF" as const
  const DIGITAL_PENDING = "PENDING" as const

  type DigitalStableLabel = typeof DIGITAL_ON | typeof DIGITAL_OFF

  function toDigitalLabel(value: unknown): DigitalStableLabel {
    return value ? DIGITAL_ON : DIGITAL_OFF
  }

  function formatPendingSuffix(active = true) {
    return active ? ` (${DIGITAL_PENDING})` : ""
  }

  function summarizeDoChannels(deviceId: number) {
    const doChannels = channelsByDevice(deviceId).filter(
      ch => ch.type === CHANNEL_TYPES.DO,
    ) as DoChannel[]
    let on = 0
    let off = 0
    doChannels.forEach(ch => {
      if (ch.state) {
        on += 1
      } else {
        off += 1
      }
    })
    return { on, off, total: doChannels.length }
  }

  function summarizeMaskTargets(doChannels: DoChannel[], mask: number) {
    let on = 0
    let off = 0
    doChannels.forEach(ch => {
      const target = ((mask >> ch.index) & 1) === 1
      if (target) {
        on += 1
      } else {
        off += 1
      }
    })
    return { on, off, total: doChannels.length }
  }

  function formatSummary(summary: { on: number; off: number; total: number }) {
    if (summary.total === 0) {
      return "no DO channels"
    }
    return `${DIGITAL_ON}: ${summary.on}, ${DIGITAL_OFF}: ${summary.off}`
  }

  function decodePairLabels(state2b: number): [DigitalStableLabel, DigitalStableLabel] | null {
    switch (state2b & 0b11) {
      case 0b00:
        // Unknown: both outputs OFF
        return [DIGITAL_OFF, DIGITAL_OFF]
      case 0b01:
        // Open: A=ON, B=OFF
        return [DIGITAL_ON, DIGITAL_OFF]
      case 0b10:
        // Closed: A=OFF, B=ON
        return [DIGITAL_OFF, DIGITAL_ON]
      case 0b11:
        // Undefined: both outputs ON
        return [DIGITAL_ON, DIGITAL_ON]
    }
    return null
  }

  function decodePairTargets(state2b: number): [boolean, boolean] | null {
    const labels = decodePairLabels(state2b)
    if (!labels) return null
    return [labels[0] === DIGITAL_ON, labels[1] === DIGITAL_ON]
  }

  type ChannelLogPayload = Pick<ChannelLogEntry, "type" | "message" | "reason">

  function pushDeviceLog(deviceId: number, entry: ChannelLogPayload, actionId?: string) {
    logStore.push(deviceId, {
      ...entry,
      actionId,
    })
  }

  function summarizeDoDiagnostics(deviceId: number, diag: DoDiagnosticsBitmasks): string {
    const doChannels = channelsByDevice(deviceId).filter(
      ch => ch.type === CHANNEL_TYPES.DO,
    ) as DoChannel[]
    if (!doChannels.length) {
      return "no DO channels"
    }

    const counts = { open: 0, fault: 0, soft: 0 }
    doChannels.forEach(ch => {
      const bit = 1 << ch.index
      if (diag.open_mask & bit) counts.open += 1
      if (diag.fault_mask & bit) counts.fault += 1
      if (diag.soft_mask & bit) counts.soft += 1
    })

    return `open:${counts.open}, fault:${counts.fault}, soft:${counts.soft}`
  }

  function logDiDiagnosticChanges(deviceId: number, changes: DiDiagnosticsChange[]) {
    changes.forEach(change => {
      const { labels, alert } = describeDiDiagnosticUpdates(change.updates)
      if (!labels.length) {
        return
      }
      const channelLabel = resolveChannelLabel(change.channel)
      const prefix = `DI ${channelLabel}`
      pushDeviceLog(deviceId, {
        type: alert ? "error" : "state",
        message: `${prefix}: ${labels.join(", ")}`
      })
    })
  }

  function ensureDoUi(channel: DoChannel): DoChannelUiState {
    if (!channel.ui) {
      channel.ui = { stage: "idle" }
    }
    return channel.ui
  }

  function clearDoUiTimers(ui: DoChannelUiState) {
    if (ui.debounceTimer) {
      clearTimeout(ui.debounceTimer)
      ui.debounceTimer = null
    }
    if (ui.timeoutTimer) {
      clearTimeout(ui.timeoutTimer)
      ui.timeoutTimer = null
    }
    if (ui.errorTimer) {
      clearTimeout(ui.errorTimer)
      ui.errorTimer = null
    }
  }

  function resetDoUiState(channel: DoChannel) {
    const ui = ensureDoUi(channel)
    clearDoUiTimers(ui)
    ui.stage = "idle"
    ui.target = undefined
    ui.previous = undefined
    ui.actionId = undefined
  }

  function enterDoPendingState(channel: DoChannel, target: boolean, actionId?: string) {
    const ui = ensureDoUi(channel)
    clearDoUiTimers(ui)
    ui.stage = "debounce"
    ui.target = target
    ui.previous = channel.state
    ui.actionId = actionId

    ui.debounceTimer = setTimeout(() => {
      ui.stage = "pending"
    }, COMMAND_PENDING_DEBOUNCE_MS)

    ui.timeoutTimer = setTimeout(() => {
      ui.stage = "error"
      ui.target = undefined
      if (ui.actionId) {
        removeAction(channel.device_id, ui.actionId)
      }
      if (typeof ui.previous === "boolean") {
        channel.state = ui.previous
      }
      ui.errorTimer = setTimeout(() => {
        resetDoUiState(channel)
      }, COMMAND_FAILURE_DISPLAY_MS)
    }, COMMAND_TIMEOUT_MS)
  }

  function fulfillDoPendingState(channel: DoChannel, actualState: boolean) {
    const ui = channel.ui
    if (!ui) {
      return
    }
    const actionId = ui.actionId

    if (ui.target === actualState || ui.stage === "error") {
      if (actionId) {
        removeAction(channel.device_id, actionId)
      }
      resetDoUiState(channel)
      return
    }

    if (ui.stage !== "idle") {
      if (actionId) {
        removeAction(channel.device_id, actionId)
      }
      clearDoUiTimers(ui)
      ui.stage = "idle"
      ui.target = undefined
      ui.previous = undefined
      ui.actionId = undefined
    }
  }

  function findDoChannel(deviceId: number, chIndex: number): DoChannel | undefined {
    const raw = channelByDeviceAndIndex(deviceId, chIndex)
    if (!raw || raw.type !== CHANNEL_TYPES.DO) {
      return undefined
    }
    return raw as DoChannel | undefined
  }

  /* ----------------------------- FETCH ALL ----------------------------- */

  async function fetchAll() {
    isLoading.value = true
    try {
      const { data } = await ChannelsAPI.list()
      const payload = Array.isArray(data)
        ? data
        : (Array.isArray((data as ChannelListDto).items)
          ? (data as ChannelListDto).items
          : [])
      const existingByDeviceAndIndex = new Map<string, Channel>()
      channels.value.forEach((channel) => {
        existingByDeviceAndIndex.set(channelKey(channel.device_id, channel.index), channel)
      })

      channels.value = payload
        .map(dto => normalizeChannel(dto))
        .map((next) => {
          const previous = existingByDeviceAndIndex.get(channelKey(next.device_id, next.index))
          return preserveRuntimeState(next, previous)
        })
      rebuildChannelIndexes()
      isLoaded.value = true
      logger.info(`📡 Loaded ${payload.length} channels`)
    } catch (err) {
      logger.error("Failed to load channels", err)
      throw err
    } finally {
      isLoading.value = false
    }
  }

  async function ensureLoaded() {
    if ((!isLoaded.value || channels.value.length === 0) && !isLoading.value) {
      await fetchAll()
    }
  }

  async function fetchByDevice(deviceId: number, force = false) {
    if (!Number.isFinite(deviceId) || deviceId <= 0) {
      return
    }
    if (!force && deviceLoaded.has(deviceId)) {
      return
    }
    if (deviceLoading.has(deviceId)) {
      return
    }

    deviceLoading.add(deviceId)
    try {
      const { data } = await DevicesAPI.getChannels(deviceId, { limit: 1000, offset: 0 })
      const payload = Array.isArray(data)
        ? data
        : (Array.isArray((data as ChannelListDto).items)
          ? (data as ChannelListDto).items
          : [])
      setBaseChannels(deviceId, payload)
      deviceLoaded.add(deviceId)
    } catch (err) {
      logger.error(`Failed to load channels for device ${deviceId}`, err)
      throw err
    } finally {
      deviceLoading.delete(deviceId)
    }
  }

  async function ensureDeviceChannelsLoaded(deviceId: number) {
    await fetchByDevice(deviceId, false)
  }

  function invalidateDeviceChannels(deviceId: number) {
    deviceLoaded.delete(deviceId)
  }

  /* ------------------------------ QUERIES ------------------------------ */

  function channelsByDevice(deviceId: number) {
    // Keep Vue dependency on reactive source; maps themselves are not tracked.
    void channels.value.length
    return [...channelsByDeviceFast(deviceId)]
  }

  function hasDeviceChannels(deviceId: number): boolean {
    return channelsByDeviceFast(deviceId).length > 0
  }

  /* ------------------------- MUTATIONS / PATCH ------------------------- */

  async function updateChannelField(id: number, changes: Partial<ChannelDto>) {
    try {
      const { data } = await ChannelsAPI.update(id, changes)
      const updated = normalizeChannel(data)
      const idx = channels.value.findIndex(c => c.id === id)
      if (idx !== -1) {
        channels.value[idx] = updated
        rebuildChannelIndexes()
      }
      logger.debug(`Channel ${id} updated`, updated)
    } catch (error) {
      logger.error(`Failed to update channel ${id}`, error)
    }
  }

  function reset() {
    for (const timer of doStateRefreshTimers.values()) {
      clearTimeout(timer)
    }
    doStateRefreshTimers.clear()
    deviceLastStateAt.clear()
    channels.value.forEach(ch => {
      if (ch.type === CHANNEL_TYPES.DO && ch.ui) {
        clearDoUiTimers(ch.ui)
      }
    })
    channels.value = []
    rebuildChannelIndexes()
    responses.value = {}
    stateRevision.value = 0
    lastStateChangedKeys.value = new Set()
    isLoaded.value = false
    deviceLoaded.clear()
    deviceLoading.clear()
  }

  function applyInitialChannels(deviceId: number, list: Channel[]) {
    const next: Channel[] = []
    for (const ch of channels.value) {
      if (ch.device_id !== deviceId) {
        next.push(ch)
        continue
      }
      if (ch.type === CHANNEL_TYPES.DO && ch.ui) {
        clearDoUiTimers(ch.ui)
      }
    }
    for (const ch of list) {
      next.push(ch)
    }
    channels.value = next
    rebuildChannelIndexes()
  }

  function preserveRuntimeState(next: Channel, previous?: Channel): Channel {
    if (!previous || previous.type !== next.type) {
      return next
    }

    if (next.type === CHANNEL_TYPES.DO && previous.type === CHANNEL_TYPES.DO) {
      const nextDo = next as DoChannel
      const previousDo = previous as DoChannel
      return {
        ...nextDo,
        state: previousDo.state,
        diagnostics: previousDo.diagnostics ? { ...previousDo.diagnostics } : undefined,
      }
    }

    if (next.type === CHANNEL_TYPES.DI && previous.type === CHANNEL_TYPES.DI) {
      const nextDi = next as DiChannel
      const previousDi = previous as DiChannel
      return {
        ...nextDi,
        state: previousDi.state,
        diDiagnostics: previousDi.diDiagnostics ? { ...previousDi.diDiagnostics } : undefined,
      }
    }

    if (next.type === CHANNEL_TYPES.AO && previous.type === CHANNEL_TYPES.AO) {
      const nextAo = next
      const previousAo = previous
      return {
        ...nextAo,
        state: previousAo.state,
      }
    }

    return next
  }

  function setBaseChannels(deviceId: number, base: Array<Channel | ChannelDto>) {
    const deviceStore = useDeviceStore()
    const fallbackType = deviceStore.devices.find(device => device.id === deviceId)?.device_type ?? null
    const existingByIndex = new Map<number, Channel>()
    for (const channel of channelsByDeviceFast(deviceId)) {
      existingByIndex.set(channel.index, channel)
    }
    const prepared = base.map((raw) => {
      const normalized = ensureChannel(raw, fallbackType)
      const next = {
        ...normalized,
        // Defensive normalization: payloads can come from different WS/REST shapes.
        // For per-device hydration, the requested device is the source of truth.
        device_id: deviceId,
      }
      return preserveRuntimeState(next, existingByIndex.get(next.index))
    })
    applyInitialChannels(deviceId, prepared)
    deviceLoaded.add(deviceId)
    logger.info(`📡 Base channels set for ${deviceId}`, prepared)
  }

  function applyBitState(deviceId: number, chIndex: number, value: boolean): { changed: boolean; actionId?: string } {
    const ch = channelByDeviceAndIndex(deviceId, chIndex)
    if (!ch) {
      return { changed: false }
    }
    if (ch.type === CHANNEL_TYPES.AO) {
      return { changed: false }
    }
    let actionId: string | undefined
    if (ch.type === CHANNEL_TYPES.DO) {
      actionId = ch.ui?.actionId
    }
    const previous = ch.state
    const changed = previous !== value
    if (changed) {
      ch.state = value
    }
    if (ch.type === CHANNEL_TYPES.DO) {
      fulfillDoPendingState(ch as DoChannel, value)
    }
    return { changed, actionId }
  }

  function applyBitmaskState(
    deviceId: number,
    mask: number,
  ): { changed: boolean; actionIds: Set<string>; changedIndexes: number[] } {
    let changed = false
    const actionIds = new Set<string>()
    const changedIndexes: number[] = []
    for (const ch of channelsByDeviceFast(deviceId)) {
      if (ch.type === CHANNEL_TYPES.AO) continue
      const next = ((mask >> ch.index) & 1) === 1
      if (ch.state !== next) {
        ch.state = next
        changed = true
        changedIndexes.push(ch.index)
      }
      if (ch.type === CHANNEL_TYPES.DO) {
        const id = ch.ui?.actionId
        if (id) {
          actionIds.add(id)
        }
        fulfillDoPendingState(ch as DoChannel, next)
      }
    }
    return { changed, actionIds, changedIndexes }
  }

  function countBits(mask: number): number {
    let value = mask >>> 0
    let count = 0
    while (value) {
      value &= value - 1
      count += 1
    }
    return count
  }

  function applyFloatState(deviceId: number, chIndex: number, value: number): { changed: boolean; actionId?: string } {
    const ch = channelByDeviceAndIndex(deviceId, chIndex)
    if (!ch || ch.type !== CHANNEL_TYPES.AO) {
      return { changed: false }
    }
    const previous = ch.state
    const changed = previous !== value
    if (changed) {
      ch.state = value
    }
    const actionId = peekAoAction(deviceId, chIndex)
    return { changed, actionId }
  }

  function setChannels(event: DeviceStateEvent) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === event.unit_id)
    if (!device) {
      logger.warn(`Device with unit_id=${event.unit_id} not found`)
      return
    }
    deviceLastStateAt.set(device.id, Date.now())
    let stateChanged = false
    const changedIndexes: number[] = []

    switch (event.mode) {
      case StateMode.STATE_SINGLE_BIT: {
        const { changed, actionId } = applyBitState(device.id, event.payload.ch, !!event.payload.value)
        if (!changed) {
          break
        }
        stateChanged = true
        changedIndexes.push(event.payload.ch)
        const singleLabel = toDigitalLabel(event.payload.value)
        pushDeviceLog(device.id, {
          type: "state",
          message: `CH${event.payload.ch + 1} state ${singleLabel}`
        }, actionId)
        if (actionId) {
          removeAction(device.id, actionId)
        }
        break
      }
      case StateMode.STATE_ALL_BIT: {
        const { changed, actionIds, changedIndexes: indexes } = applyBitmaskState(device.id, event.payload.bitmask)
        if (!changed) {
          break
        }
        stateChanged = true
        changedIndexes.push(...indexes)
        const deviceType = device.device_type.toLowerCase()
        if (deviceType === "do") {
          const maskSummary = formatSummary(summarizeDoChannels(device.id))
          const actionId = actionIds.size === 1 ? Array.from(actionIds)[0] : undefined
          pushDeviceLog(device.id, {
            type: "state",
            message: `All digital outputs updated (${maskSummary})`
          }, actionId)
          if (actionId) {
            removeAction(device.id, actionId)
          }
        }
        break
      }
      case StateMode.STATE_SINGLE_FLOAT: {
        const { changed, actionId } = applyFloatState(device.id, event.payload.ch, Number(event.payload.value))
        if (!changed) {
          break
        }
        stateChanged = true
        changedIndexes.push(event.payload.ch)
        const aoValue = formatAoValue(Number(event.payload.value))
        pushDeviceLog(device.id, {
          type: "state",
          message: `AO CH${event.payload.ch + 1} set to ${aoValue} mA`
        }, actionId)
        if (actionId) {
          clearAoAction(device.id, event.payload.ch)
          removeAction(device.id, actionId)
        }
        break
      }
      case StateMode.DIAG_ALL_BIT: {
        const diagPayload: DoDiagnosticsBitmasks = {
          open_mask: Number(event.payload.open_mask) >>> 0,
          fault_mask: Number(event.payload.fault_mask) >>> 0,
          soft_mask: Number(event.payload.soft_mask) >>> 0,
        }
        const doChannels = channelsByDevice(device.id).filter(
          ch => ch.type === CHANNEL_TYPES.DO,
        ) as DoChannel[]
        const changed = applyDoDiagnostics(doChannels, diagPayload)
        if (!changed) {
          break
        }
        const summary = summarizeDoDiagnostics(device.id, diagPayload)
        pushDeviceLog(device.id, {
          type: "state",
          message: `Diagnostics updated (${summary})`
        })
        break
      }
      case StateMode.STATE_CHANGED_BIT: {
        const changedMask = Number(event.payload.changed) >>> 0
        const stateMask = Number(event.payload.state) >>> 0
        const { changed, actionIds, updates } = applyDeltaState(
          channels.value,
          device.id,
          changedMask,
          stateMask,
          { onDoUpdate: (channel, next) => fulfillDoPendingState(channel, next) },
        )
        if (!changed) {
          break
        }
        stateChanged = true
        for (const update of updates) {
          changedIndexes.push(update.channel.index)
        }
        const channelDescriptions = updates
          .map(({ channel, value }) => `${resolveChannelLabel(channel)} → ${toDigitalLabel(value)}`)
        const message = channelDescriptions.length
          ? `Digital delta: ${channelDescriptions.join(", ")}`
          : `Digital delta (${countBits(changedMask)} ch)`
        const actionId = actionIds.size === 1 ? Array.from(actionIds)[0] : undefined
        pushDeviceLog(device.id, {
          type: "state",
          message
        }, actionId)
        if (actionId) {
          removeAction(device.id, actionId)
        }
        break
      }
      case StateMode.DIAG_DI_BIT: {
        const diagPayload: DiDiagnosticsBitmasks = {
          seen_mask: Number(event.payload.seen) >>> 0,
          stuck_mask: Number(event.payload.stuck) >>> 0,
          lost_mask: Number(event.payload.lost) >>> 0,
          latched_mask: Number(event.payload.latched) >>> 0,
          latched_changed_mask: Number(event.payload.latched_changed) >>> 0,
          latched_cause_mask: Number(event.payload.latched_cause) >>> 0,
        }
        const diChannels = channelsByDevice(device.id).filter(
          ch => ch.type === CHANNEL_TYPES.DI,
        ) as DiChannel[]
        const changes = applyDiDiagnostics(diChannels, diagPayload)
        if (!changes.length) {
          break
        }
        logDiDiagnosticChanges(device.id, changes)
        break
      }
      case StateMode.STATE_LATCHED_BIT: {
        const diagPayload: DiDiagnosticsBitmasks = {
          latched_mask: Number(event.payload.latched) >>> 0,
          latched_changed_mask: Number(event.payload.changed) >>> 0,
          latched_cause_mask: Number(event.payload.cause) >>> 0,
        }
        const diChannels = channelsByDevice(device.id).filter(
          ch => ch.type === CHANNEL_TYPES.DI,
        ) as DiChannel[]
        const changes = applyDiDiagnostics(diChannels, diagPayload)
        if (!changes.length) {
          break
        }
        logDiDiagnosticChanges(device.id, changes)
        break
      }
      default:
        logger.debug(`Unhandled device state mode=${event.mode}`)
    }

    if (stateChanged) {
      lastStateChangedKeys.value = new Set(
        changedIndexes.map(index => channelKey(device.id, index)),
      )
      stateRevision.value += 1
    }
  }

  function didChannelChangeInLastRevision(deviceId: number, chIndex: number): boolean {
    return lastStateChangedKeys.value.has(channelKey(deviceId, chIndex))
  }

  function setResponse(resp: DeviceRespEvent) {
    responses.value = {
      ...responses.value,
      [resp.unit_id]: resp,
    }

    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === resp.unit_id)

    if (!device) {
      logger.warn(`DeviceRespEvent: device not found for unit_id=${resp.unit_id}`)
      return
    }

    // Runtime source of truth for channel states is DEVICE_STATE only.
    // DEVICE_RESP is transport-level diagnostics; it must not mutate channel state.
    if (resp.status === "OK") {
      logger.debug(`✅ Device response from ${resp.unit_id}, packet=${resp.packet_id}, status=${resp.status}`)
    } else {
      logger.debug(
        `⚠️ Command resp from ${resp.unit_id}, packet=${resp.packet_id}, status=${resp.status}, error=${resp.error}`,
      )
      logger.warn(
        `⚠️ Command response from ${resp.unit_id}: status=${resp.status}${resp.error ? `, error=${resp.error}` : ""}`,
      )
      const errorDetail = resp.error ? `: ${resp.error}` : ""
      pushDeviceLog(device.id, {
        type: "error",
        message: `Command error (${resp.status})${errorDetail}`
      })
    }
  }

  function requestStates(
    deviceId: number,
    options: { includeDiagnostics?: boolean; silent?: boolean } = {},
  ) {
    const {
      includeDiagnostics = true,
      silent = false,
    } = options
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.id === deviceId)
    if (!device) {
      logger.error(`Device ${deviceId} not found for requestStates`)
      return
    }

    const ws = useWebSocketStore()
    const lowerType = device.device_type.toLowerCase()
    const msg: RequestStateMessage = {
      action: WSAction.GET_STATES,
      unit_id: device.unit_id,
      mode:
        lowerType === "ao"
          ? ReqStateMode.REQ_ALL_FLOAT
          : ReqStateMode.REQ_ALL_BIT,
    }
    ws.send(msg)
    if (!silent) {
      logger.info(`Requested states from ${device.unit_id}`)
    }

    if (includeDiagnostics && lowerType === "do") {
      ws.send({
        action: WSAction.GET_STATES,
        unit_id: device.unit_id,
        mode: ReqStateMode.REQ_DIAG_ALL_BIT,
      } satisfies RequestStateMessage)
      if (!silent) {
        logger.info(`Requested DO diagnostics from ${device.unit_id}`)
      }
    } else if (includeDiagnostics && lowerType === "di") {
      ws.send({
        action: WSAction.GET_STATES,
        unit_id: device.unit_id,
        mode: ReqStateMode.REQ_DIAG_DI_BIT,
      } satisfies RequestStateMessage)
      if (!silent) {
        logger.info(`Requested DI diagnostics from ${device.unit_id}`)
      }
    }
  }

  /* ----------------------------- COMMANDS ----------------------------- */

  function hasPendingDoForDevice(deviceId: number): boolean {
    for (const channel of channelsByDeviceFast(deviceId)) {
      if (channel.type !== CHANNEL_TYPES.DO) {
        continue
      }
      const stage = (channel as DoChannel).ui?.stage
      if (stage === "pending" || stage === "debounce") {
        return true
      }
    }
    return false
  }

  function scheduleDoStateRefreshIfPending(deviceId: number, commandIssuedAt: number) {
    const existing = doStateRefreshTimers.get(deviceId)
    if (existing) {
      clearTimeout(existing)
    }
    const timer = setTimeout(() => {
      doStateRefreshTimers.delete(deviceId)
      if (!hasPendingDoForDevice(deviceId)) {
        return
      }
      const lastStateAt = deviceLastStateAt.get(deviceId) ?? 0
      if (lastStateAt >= commandIssuedAt) {
        return
      }
      requestStates(deviceId, { includeDiagnostics: false, silent: true })
    }, 360)
    doStateRefreshTimers.set(deviceId, timer)
  }

  function sendDoCommand(unitId: string, chIndex: number, state: boolean) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      logger.error(`Device ${unitId} not found for DO command`)
      return
    }

    const actionId = enqueueAction(device.id)

    const channel = findDoChannel(device.id, chIndex)
    if (channel) {
      enterDoPendingState(channel, state, actionId)
    }

    // applyBitState(device.id, chIndex, state)

    const ws = useWebSocketStore()
    const msg: SetDoCommandMessage = {
      action: WSAction.SET_DO_COMMAND,
      unit_id: device.unit_id,
      mode: CmdMode.SET_SINGLE_BIT,
      ch: chIndex,
      value: state ? 1 : 0,
    }
    const commandIssuedAt = Date.now()
    ws.send(msg)
    scheduleDoStateRefreshIfPending(device.id, commandIssuedAt)
    const targetLabel = toDigitalLabel(state)
    logger.info(`➡️ DO cmd ${device.unit_id} ch=${chIndex} → ${targetLabel}`)

    pushDeviceLog(device.id, {
      type: "cmd",
      message: `User requested DO CH${chIndex + 1} → ${targetLabel}${formatPendingSuffix(true)}`
    }, actionId)
  }

  function sendDoAllCommand(deviceId: number, unitId: string, mask: number) {
    const deviceStore = useDeviceStore()
    const device =
      deviceStore.devices.find(d => d.id === deviceId) ||
      deviceStore.devices.find(d => d.unit_id === unitId)

    if (!device) {
      logger.error(`Device ${unitId} not found for DO all command`)
      return
    }

    const doChannels = channelsByDevice(device.id).filter(
      ch => ch.type === CHANNEL_TYPES.DO,
    ) as DoChannel[]

    const actionId = enqueueAction(device.id)

    doChannels.forEach(ch => {
      const target = ((mask >> ch.index) & 1) === 1
      enterDoPendingState(ch, target, actionId)
    })
    const maskSummary = formatSummary(summarizeMaskTargets(doChannels, mask))

    const ws = useWebSocketStore()
    const commandIssuedAt = Date.now()
    ws.send({
      action: WSAction.SET_DO_COMMAND,
      unit_id: unitId,
      mode: CmdMode.SET_ALL_BIT,
      bitmask: mask,
    } satisfies SetDoCommandMessage)
    scheduleDoStateRefreshIfPending(device.id, commandIssuedAt)
    logger.info(`➡️ DO ALL cmd ${unitId} targets → ${maskSummary}`)

    pushDeviceLog(device.id, {
      type: "cmd",
      message: `User requested DO ALL (${maskSummary})${formatPendingSuffix(doChannels.length > 0)}`,
    }, actionId)
  }

  function sendDoPairCommand(
    unitId: string,
    chA: number,
    chB: number,
    state2b: 0 | 1 | 2 | 3,
    options: { source?: string } = {},
  ): { ok: true; deviceId: number; actionId: string } | { ok: false; error: string } {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      const error = `Device ${unitId} not found for DO pair command`
      logger.error(error)
      return { ok: false, error }
    }
    if (chA === chB) {
      const error = `Invalid DO pair command for ${unitId}: channels must differ (ch=${chA})`
      logger.error(error)
      return { ok: false, error }
    }

    const pairTargets = decodePairTargets(state2b)
    if (!pairTargets) {
      const error = `Invalid DO pair state code for ${unitId}: ${state2b}`
      logger.error(error)
      return { ok: false, error }
    }

    const channelA = findDoChannel(device.id, chA)
    const channelB = findDoChannel(device.id, chB)
    if (!channelA || !channelB) {
      const error = `DO pair channels not found on ${unitId}: [${chA}/${chB}]`
      logger.error(error)
      return { ok: false, error }
    }

    const actionId = enqueueAction(device.id)

    enterDoPendingState(channelA, pairTargets[0], actionId)
    enterDoPendingState(channelB, pairTargets[1], actionId)

    const ws = useWebSocketStore()
    const commandIssuedAt = Date.now()
    ws.send({
      action: WSAction.SET_DO_COMMAND,
      unit_id: device.unit_id,
      mode: CmdMode.SET_PAIR_BIT,
      chA,
      chB,
      state2b,
    } satisfies SetDoCommandMessage)
    scheduleDoStateRefreshIfPending(device.id, commandIssuedAt)
    const pairLabels = decodePairLabels(state2b)
    const labelA = pairLabels?.[0] ?? "N/A"
    const labelB = pairLabels?.[1] ?? "N/A"
    const source = options.source?.trim() || "unknown"
    logger.info(
      `➡️ DO pair cmd ${device.unit_id} [${chA}/${chB}] → CH${chA + 1}:${labelA} / CH${chB + 1}:${labelB} (src=${source})`,
    )
    pushDeviceLog(device.id, {
      type: "cmd",
      message: `User requested DO pair CH${chA + 1} → ${labelA}, CH${chB + 1} → ${labelB}${formatPendingSuffix(Boolean(pairLabels))} [${source}]`
    }, actionId)
    return { ok: true, deviceId: device.id, actionId }
  }

  function sendAoCommand(unitId: string, chIndex: number, value: number) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      logger.error(`Device ${unitId} not found for AO command`)
      return
    }

    // applyFloatState(device.id, chIndex, value)

    const actionId = enqueueAction(device.id)
    registerAoAction(device.id, chIndex, actionId)

    const ws = useWebSocketStore()
    ws.send({
      action: WSAction.SET_AO_COMMAND,
      unit_id: device.unit_id,
      ch: chIndex,
      value,
    } satisfies SetAoCommandMessage)
    const aoValue = formatAoValue(value)
    logger.info(`➡️ AO cmd ${device.unit_id} ch=${chIndex} → ${aoValue} mA`)

    pushDeviceLog(device.id, {
      type: "cmd",
      message: `User requested AO CH${chIndex + 1} → ${aoValue} mA`
    }, actionId)
  }

  /* --------------------------- RESOLVERS --------------------------- */

  function resolveUnitId(deviceId: number): string {
    const deviceStore = useDeviceStore()
    const dev = deviceStore.devices.find(d => d.id === deviceId)
    return dev?.unit_id ?? `dev#${deviceId}`
  }

  function resolveUnitName(deviceId: number): string {
    const deviceStore = useDeviceStore()
    const dev = deviceStore.devices.find(d => d.id === deviceId)
    return dev?.name?.trim() || dev?.unit_id || `dev#${deviceId}`
  }

  function resolveChannelLabel(ch: Channel): string {
    if (ch.resolved_name?.trim()) {
      return ch.resolved_name
    }
    if (ch.name?.trim()) {
      return ch.name
    }
    return `CH${ch.index + 1}`
  }

  function resolveChannelFullLabel(ch: Channel): string {
    return `${resolveUnitName(ch.device_id)}/${resolveChannelLabel(ch)}`
  }

  function hasPendingCommandForUnit(unitId: string): boolean {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      return false
    }
    return hasPendingDoForDevice(device.id)
  }

  return {
    channels,
    responses,
    stateRevision,
    isLoading,
    isLoaded,

    fetchAll,
    ensureLoaded,
    ensureDeviceChannelsLoaded,
    invalidateDeviceChannels,
    reset,

    channelsByDevice,
    hasDeviceChannels,
    setBaseChannels,
    setChannels,
    setResponse,
    requestStates,

    updateChannelField,

    sendDoCommand,
    sendDoAllCommand,
    sendDoPairCommand,
    sendAoCommand,

    resolveUnitId,
    resolveUnitName,
    resolveChannelLabel,
    resolveChannelFullLabel,
    didChannelChangeInLastRevision,
    hasPendingCommandForUnit,
  }
})
