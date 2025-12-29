// src/stores/channelStore.ts
import { defineStore } from "pinia"
import { ref } from "vue"

import { ChannelsAPI } from "@/api/channels.api"
import { getLogger } from "@/utils/logger"
import { normalizeChannel, ensureChannel, formatAoValue } from "@/utils/channel"

import { useDeviceStore } from "@/stores/deviceStore"
import { useWebSocketStore } from "@/stores/websocketStore"

import { useChannelLogStore } from "@/stores/channelLogStore"

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

import { CHANNEL_TYPES, type Channel, type ChannelDto, type DoChannel, type DoChannelUiState } from "@/types/channel"

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

  const DIGITAL_ON = "ON"
  const DIGITAL_OFF = "OFF"
  const DIGITAL_PENDING = "PENDING"

  function toDigitalLabel(value: unknown): "ON" | "OFF" {
    return value ? DIGITAL_ON : DIGITAL_OFF
  }

  function formatPendingSuffix() {
    return ` (${DIGITAL_PENDING})`
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

  function decodePairLabels(state2b: number): ["ON" | "OFF", "ON" | "OFF"] | null {
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
  }

  function enterDoPendingState(channel: DoChannel, target: boolean) {
    const ui = ensureDoUi(channel)
    clearDoUiTimers(ui)
    ui.stage = "debounce"
    ui.target = target
    ui.previous = channel.state

    ui.debounceTimer = setTimeout(() => {
      ui.stage = "pending"
    }, COMMAND_PENDING_DEBOUNCE_MS)

    ui.timeoutTimer = setTimeout(() => {
      ui.stage = "error"
      ui.target = undefined
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

  function applyBitState(deviceId: number, chIndex: number, value: boolean) {
    for (const ch of channels.value) {
      if (ch.device_id === deviceId && ch.index === chIndex) {

        if (ch.type === CHANNEL_TYPES.AO) {
          break
        }
        if (ch.state !== value) {
          ch.state = value
        }
        if (ch.type === CHANNEL_TYPES.DO) {
          fulfillDoPendingState(ch as DoChannel, value)
        }
        break
      }
    }
  }

  function applyBitmaskState(deviceId: number, mask: number) {
    for (const ch of channels.value) {
      if (ch.device_id !== deviceId || ch.type === CHANNEL_TYPES.AO) continue
      const next = ((mask >> ch.index) & 1) === 1
      if (ch.state !== next) {
        ch.state = next
      }
      if (ch.type === CHANNEL_TYPES.DO) {
        fulfillDoPendingState(ch as DoChannel, next)
      }
    }

  }

  function applyFloatState(deviceId: number, chIndex: number, value: number) {
    for (const ch of channels.value) {
      if (ch.device_id === deviceId && ch.index === chIndex && ch.type === CHANNEL_TYPES.AO) {

        if (ch.state !== value) {
          ch.state = value
        }
        break
      }
    }
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
        applyBitState(device.id, event.payload.ch, !!event.payload.value)
        const singleLabel = toDigitalLabel(event.payload.value)
        logStore.push(device.id, {
          type: "state",
          message: `CH${event.payload.ch + 1} state ${singleLabel}`
        })
        break
      }
      case StateMode.STATE_ALL_BIT: {
        applyBitmaskState(device.id, event.payload.bitmask)
        const maskSummary = formatSummary(summarizeDoChannels(device.id))
        logStore.push(device.id, {
          type: "state",
          message: `All digital outputs updated (${maskSummary})`
        })
        break
      }
      case StateMode.STATE_SINGLE_FLOAT: {
        applyFloatState(device.id, event.payload.ch, Number(event.payload.value))
        const aoValue = formatAoValue(Number(event.payload.value))
        logStore.push(device.id, {
          type: "state",
          message: `AO CH${event.payload.ch + 1} set to ${aoValue} mA`
        })
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

    if (resp.status === "OK") {
      logger.info(`✅ Command ack from ${resp.unit_id}, packet=${resp.packet_id}`)
      logStore.push(device.id, {
        type: "resp",
        message: `ACK packet=${resp.packet_id}`
      })
    } else {
      logger.warn(
        `⚠️ Command resp from ${resp.unit_id}, packet=${resp.packet_id}, status=${resp.status}, error=${resp.error}`,
      )
      logStore.push(device.id, {
        type: "error",
        message: `RESP ERROR packet=${resp.packet_id} → ${resp.error}`
      })
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
    const msg: RequestStateMessage = {
      action: WSAction.GET_STATES,
      unit_id: device.unit_id,
      mode:
        device.device_type.toLowerCase() === "ao"
          ? ReqStateMode.REQ_ALL_FLOAT
          : ReqStateMode.REQ_ALL_BIT,
    }
    ws.send(msg)
    logger.info(`Requested states from ${device.unit_id}`)
  }

  /* ----------------------------- COMMANDS ----------------------------- */

  function sendDoCommand(unitId: string, chIndex: number, state: boolean) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      logger.error(`Device ${unitId} not found for DO command`)
      return
    }

    const channel = findDoChannel(device.id, chIndex)
    if (channel) {
      enterDoPendingState(channel, state)
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

    logStore.push(device.id, {
      type: "cmd",
      message: `CMD DO CH${chIndex + 1} ${targetLabel}${formatPendingSuffix()}`
    })
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

    doChannels.forEach(ch => {
      const target = ((mask >> ch.index) & 1) === 1
      enterDoPendingState(ch, target)
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

    logStore.push(device.id, {
      type: "cmd",
      message: `CMD DO ALL ${maskSummary}${formatPendingSuffix()}`,
    })
  }

  function sendDoPairCommand(unitId: string, chA: number, chB: number, state2b: 0 | 1 | 2 | 3) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      logger.error(`Device ${unitId} not found for DO pair command`)
      return
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
    const pairLabels = decodePairLabels(state2b) ?? [DIGITAL_PENDING, DIGITAL_PENDING]
    logger.info(
      `➡️ DO pair cmd ${device.unit_id} [${chA}/${chB}] → CH${chA + 1}:${pairLabels[0]} / CH${chB + 1}:${pairLabels[1]}`,
    )
    logStore.push(device.id, {
      type: "cmd",
      message: `CMD DO PAIR CH${chA + 1}=${pairLabels[0]}, CH${chB + 1}=${pairLabels[1]}${formatPendingSuffix()}`
    })
  }

  function sendAoCommand(unitId: string, chIndex: number, value: number) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      logger.error(`Device ${unitId} not found for AO command`)
      return
    }

    // applyFloatState(device.id, chIndex, value)

    const ws = useWebSocketStore()
    ws.send({
      action: WSAction.SET_AO_COMMAND,
      unit_id: device.unit_id,
      ch: chIndex,
      value,
    } satisfies SetAoCommandMessage)
    const aoValue = formatAoValue(value)
    logger.info(`➡️ AO cmd ${device.unit_id} ch=${chIndex} → ${aoValue} mA`)

    logStore.push(device.id, {
      type: "cmd",
      message: `CMD AO CH${chIndex + 1} set to ${aoValue} mA`
    })
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
