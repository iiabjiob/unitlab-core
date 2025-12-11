// src/stores/channelStore.ts
import { defineStore } from "pinia"
import { computed, ref, shallowRef, triggerRef } from "vue"

import { ChannelsAPI } from "@/api/channels.api"
import { getLogger } from "@/utils/logger"
import { normalizeChannel, ensureChannel } from "@/utils/channel"

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

import { CHANNEL_TYPES, type Channel, type ChannelDto } from "@/types/channel"

const logger = getLogger("CHANNEL")

export const useChannelStore = defineStore("channelStore", () => {
  const channels = ref<Channel[]>([])
  const responses = ref<Record<string, DeviceRespEvent>>({})

  const isLoading = ref(false)
  const isLoaded = ref(false)

  const logStore = useChannelLogStore()

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
    channels.value = []
    responses.value = {}
    isLoaded.value = false
  }

  function applyInitialChannels(deviceId: number, list: Channel[]) {
    const next: Channel[] = []
    for (const ch of channels.value) {
      if (ch.device_id !== deviceId) {
        next.push(ch)
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
          continue
        }
        if (ch.state !== value) {
          ch.state = value
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
      case StateMode.STATE_SINGLE_BIT:
        applyBitState(device.id, event.payload.ch, !!event.payload.value)
        logStore.push(device.id, {
          type: "state",
          message: `STATE CH${event.payload.ch + 1} → ${event.payload.value}`
        })
        break
      case StateMode.STATE_ALL_BIT:
        applyBitmaskState(device.id, event.payload.bitmask)
        logStore.push(device.id, {
          type: "state",
          message: `STATE BITMASK=${event.payload.bitmask.toString(2).padStart(32, "0")}`
        })
        break
      case StateMode.STATE_SINGLE_FLOAT:
        applyFloatState(device.id, event.payload.ch, Number(event.payload.value))
        logStore.push(device.id, {
          type: "state",
          message: `STATE AO CH${event.payload.ch + 1} → ${event.payload.value}`
        })
        break
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
    logger.info(`➡️ DO cmd ${device.unit_id} ch=${chIndex} → ${state}`)

    logStore.push(device.id, {
      type: "cmd",
      message: `SEND DO CH${chIndex + 1} → ${state}`
    })
  }

  function sendDoAllCommand(unitId: string, mask: number) {
    const ws = useWebSocketStore()
    ws.send({
      action: WSAction.SET_DO_COMMAND,
      unit_id: unitId,
      mode: CmdMode.SET_ALL_BIT,
      bitmask: mask,
    } satisfies SetDoCommandMessage)
    logger.info(`➡️ DO ALL cmd ${unitId} mask=${mask}`)
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
    logger.info(`➡️ DO pair cmd ${device.unit_id} [${chA}/${chB}] → ${state2b}`)
    logStore.push(device.id, {
      type: "cmd",
      message: `SEND DO PAIR [${chA + 1}/${chB + 1}] → state2b=${state2b}`
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
    logger.info(`➡️ AO cmd ${device.unit_id} ch=${chIndex} → ${value}`)

    logStore.push(device.id, {
      type: "cmd",
      message: `SEND AO CH${chIndex + 1} → ${value}`
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
