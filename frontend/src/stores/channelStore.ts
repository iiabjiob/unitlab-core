// src/stores/channelStore.ts
import { defineStore } from "pinia"
import { ref } from "vue"

import { ChannelsAPI } from "@/api/channels.api"
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

import { CHANNEL_TYPES, type Channel, type ChannelDto, type DiChannel, type DoChannel, type DoChannelUiState } from "@/types/channel"
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

  const isLoading = ref(false)
  const isLoaded = ref(false)

  const logStore = useChannelLogStore()

  const actionCounters = new Map<number, number>()
  const actionQueues = new Map<number, string[]>()
  const aoActionMap = new Map<string, string>()

  function enqueueAction(deviceId: number): string {
    const next = (actionCounters.get(deviceId) ?? 0) + 1
    actionCounters.set(deviceId, next)
    const actionId = next.toString().padStart(4, "0")
    const queue = actionQueues.get(deviceId) ?? []
    queue.push(actionId)
    actionQueues.set(deviceId, queue)
    return actionId
  }

  function consumeAction(deviceId: number): string | undefined {
    const queue = actionQueues.get(deviceId)
    if (!queue || queue.length === 0) return undefined
    const id = queue.shift()
    if (!queue.length) {
      actionQueues.delete(deviceId)
    }
    return id
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

  function registerAoAction(deviceId: number, chIndex: number, actionId: string) {
    aoActionMap.set(aoKey(deviceId, chIndex), actionId)
  }

  function peekAoAction(deviceId: number, chIndex: number): string | undefined {
    return aoActionMap.get(aoKey(deviceId, chIndex))
  }

  function clearAoAction(deviceId: number, chIndex: number) {
    aoActionMap.delete(aoKey(deviceId, chIndex))
  }

  function purgeAoAction(actionId?: string) {
    if (!actionId) return
    for (const [key, id] of aoActionMap.entries()) {
      if (id === actionId) {
        aoActionMap.delete(key)
      }
    }
  }

  const DIGITAL_ON = "ON" as const
  const DIGITAL_OFF = "OFF" as const
  const DIGITAL_PENDING = "PENDING" as const

  type DigitalStableLabel = typeof DIGITAL_ON | typeof DIGITAL_OFF
  type DigitalLabel = DigitalStableLabel | typeof DIGITAL_PENDING

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
    switch (state2b) {
      case 0b00:
        return [DIGITAL_ON, DIGITAL_OFF]
      case 0b01:
        return [DIGITAL_OFF, DIGITAL_OFF]
      case 0b10:
        return [DIGITAL_ON, DIGITAL_ON]
      default:
        return null
    }
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

    if (ui.target === actualState || ui.stage === "error") {
      resetDoUiState(channel)
      return
    }

    if (ui.stage !== "idle") {
      clearDoUiTimers(ui)
      ui.stage = "idle"
      ui.target = undefined
      ui.previous = undefined
      ui.actionId = undefined
    }
  }

  function findDoChannel(deviceId: number, chIndex: number): DoChannel | undefined {
    const raw = channels.value.find(
      ch => ch.device_id === deviceId && ch.index === chIndex && ch.type === CHANNEL_TYPES.DO,
    )
    return raw as DoChannel | undefined
  }

  /* ----------------------------- FETCH ALL ----------------------------- */

  async function fetchAll() {
    isLoading.value = true
    try {
      const { data } = await ChannelsAPI.list()
      channels.value = data.map(normalizeChannel)
      isLoaded.value = true
      logger.info(`📡 Loaded ${data.length} channels`)
    } catch (err) {
      logger.error("Failed to load channels", err)
      throw err
    } finally {
      isLoading.value = false
    }
  }

  async function ensureLoaded() {
    if (!isLoaded.value && !isLoading.value) {
      await fetchAll()
    }
  }

  /* ------------------------------ QUERIES ------------------------------ */

  function channelsByDevice(deviceId: number) {
    return channels.value.filter(ch => ch.device_id === deviceId)
  }

  /* ------------------------- MUTATIONS / PATCH ------------------------- */

  async function updateChannelField(id: number, changes: Partial<ChannelDto>) {
    try {
      const { data } = await ChannelsAPI.update(id, changes)
      const updated = normalizeChannel(data)
      const idx = channels.value.findIndex(c => c.id === id)
      if (idx !== -1) {
        channels.value[idx] = updated
      }
      logger.debug(`Channel ${id} updated`, updated)
    } catch (error) {
      logger.error(`Failed to update channel ${id}`, error)
    }
  }

  function reset() {
    channels.value.forEach(ch => {
      if (ch.type === CHANNEL_TYPES.DO && ch.ui) {
        clearDoUiTimers(ch.ui)
      }
    })
    channels.value = []
    responses.value = {}
    isLoaded.value = false
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
  }

  function setBaseChannels(deviceId: number, base: Array<Channel | ChannelDto>) {
    const prepared = base.map(ensureChannel)
    applyInitialChannels(deviceId, prepared)
    logger.info(`📡 Base channels set for ${deviceId}`, prepared)
  }

  function applyBitState(deviceId: number, chIndex: number, value: boolean): { changed: boolean; actionId?: string } {
    for (const ch of channels.value) {
      if (ch.device_id === deviceId && ch.index === chIndex) {
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
    }
    return { changed: false }
  }

  function applyBitmaskState(deviceId: number, mask: number): { changed: boolean; actionIds: Set<string> } {
    let changed = false
    const actionIds = new Set<string>()
    for (const ch of channels.value) {
      if (ch.device_id !== deviceId || ch.type === CHANNEL_TYPES.AO) continue
      const next = ((mask >> ch.index) & 1) === 1
      if (ch.state !== next) {
        ch.state = next
        changed = true
      }
      if (ch.type === CHANNEL_TYPES.DO) {
        const id = ch.ui?.actionId
        if (id) {
          actionIds.add(id)
        }
        fulfillDoPendingState(ch as DoChannel, next)
      }
    }
    return { changed, actionIds }
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
    for (const ch of channels.value) {
      if (ch.device_id === deviceId && ch.index === chIndex && ch.type === CHANNEL_TYPES.AO) {
        const previous = ch.state
        const changed = previous !== value
        if (changed) {
          ch.state = value
        }
        const actionId = peekAoAction(deviceId, chIndex)
        return { changed, actionId }
      }
    }
    return { changed: false }
  }

  function setChannels(event: DeviceStateEvent) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === event.unit_id)
    if (!device) {
      logger.warn(`Device with unit_id=${event.unit_id} not found`)
      return
    }

    switch (event.mode) {
      case StateMode.STATE_SINGLE_BIT: {
        const { changed, actionId } = applyBitState(device.id, event.payload.ch, !!event.payload.value)
        if (!changed) {
          break
        }
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
        const { changed, actionIds } = applyBitmaskState(device.id, event.payload.bitmask)
        if (!changed) {
          break
        }
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
      case StateMode.STATE_ALL_DIAG: {
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
      case StateMode.STATE_DIAG_DI: {
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
      case StateMode.STATE_LATCHED_DI: {
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

    const actionId = consumeAction(device.id)

    if (resp.status === "OK") {
      logger.debug(`✅ Command ack from ${resp.unit_id}, packet=${resp.packet_id}`)
      logger.info(`✅ Command acknowledged by ${resp.unit_id}`)
      pushDeviceLog(device.id, {
        type: "resp",
        message: "Command acknowledged"
      }, actionId)
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
      }, actionId)
      purgeAoAction(actionId)
    }
  }

  function requestStates(deviceId: number) {
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
    logger.info(`Requested states from ${device.unit_id}`)

    if (lowerType === "do") {
      ws.send({
        action: WSAction.GET_STATES,
        unit_id: device.unit_id,
        mode: ReqStateMode.REQ_ALL_DIAG,
      } satisfies RequestStateMessage)
      logger.info(`Requested DO diagnostics from ${device.unit_id}`)
    } else if (lowerType === "di") {
      ws.send({
        action: WSAction.GET_STATES,
        unit_id: device.unit_id,
        mode: ReqStateMode.REQ_DIAG_DI,
      } satisfies RequestStateMessage)
      logger.info(`Requested DI diagnostics from ${device.unit_id}`)
    }
  }

  /* ----------------------------- COMMANDS ----------------------------- */

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
    ws.send(msg)
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
    ws.send({
      action: WSAction.SET_DO_COMMAND,
      unit_id: unitId,
      mode: CmdMode.SET_ALL_BIT,
      bitmask: mask,
    } satisfies SetDoCommandMessage)
    logger.info(`➡️ DO ALL cmd ${unitId} targets → ${maskSummary}`)

    pushDeviceLog(device.id, {
      type: "cmd",
      message: `User requested DO ALL (${maskSummary})${formatPendingSuffix(doChannels.length > 0)}`,
    }, actionId)
  }

  function sendDoPairCommand(unitId: string, chA: number, chB: number, state2b: 0 | 1 | 2 | 3) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      logger.error(`Device ${unitId} not found for DO pair command`)
      return
    }

    const actionId = enqueueAction(device.id)

    const pairTargets = decodePairTargets(state2b)
    if (pairTargets) {
      const channelA = findDoChannel(device.id, chA)
      if (channelA) {
        enterDoPendingState(channelA, pairTargets[0], actionId)
      }
      const channelB = findDoChannel(device.id, chB)
      if (channelB) {
        enterDoPendingState(channelB, pairTargets[1], actionId)
      }
    }

    const ws = useWebSocketStore()
    ws.send({
      action: WSAction.SET_DO_COMMAND,
      unit_id: device.unit_id,
      mode: CmdMode.SET_PAIR_BIT,
      chA,
      chB,
      state2b,
    } satisfies SetDoCommandMessage)
    const pairLabels = (decodePairLabels(state2b) ?? [DIGITAL_PENDING, DIGITAL_PENDING]) as [DigitalLabel, DigitalLabel]
    logger.info(
      `➡️ DO pair cmd ${device.unit_id} [${chA}/${chB}] → CH${chA + 1}:${pairLabels[0]} / CH${chB + 1}:${pairLabels[1]}`,
    )
    pushDeviceLog(device.id, {
      type: "cmd",
      message: `User requested DO pair CH${chA + 1} → ${pairLabels[0]}, CH${chB + 1} → ${pairLabels[1]}${formatPendingSuffix(!!pairTargets)}`
    }, actionId)
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

  return {
    channels,
    responses,
    isLoading,
    isLoaded,

    fetchAll,
    ensureLoaded,
    reset,

    channelsByDevice,
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
  }
})
