// src/stores/channelStore.ts
import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { Channel } from '@/types/channel'
import axios from "axios"
import { ApiBuilder } from '@/utils/api'

import {
  StateMode,
  type DeviceStateEvent,
  type DeviceRespEvent
} from '@/types/ws/events'

import {
  WSAction,
  CmdMode,
  ReqStateMode,
  type SetDoCommandMessage,
  type SetAoCommandMessage,
  type RequestStateMessage
} from '@/types/ws/messages'

import { useWebSocketStore } from "@/stores/websocketStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { getLogger } from '@/utils/logger'

const logger = getLogger('CH')

export const useChannelStore = defineStore('channelStore', () => {
  const channels = ref<Channel[]>([])
  const responses = ref<Record<string, DeviceRespEvent>>({})

  function channelsByDevice(deviceId: number) {
    return channels.value.filter(ch => ch.device_id === deviceId)
  }

  // PATCH /channels/{id}
  async function updateChannelField(id: number, changes: Partial<Channel>) {
    try {
      const { data } = await axios.patch(ApiBuilder.channel(id), changes)
      const idx = channels.value.findIndex(c => c.id === id)
      if (idx !== -1) {
        channels.value[idx] = data
      }
      logger.debug(`✅ Channel ${id} updated with`, changes)
    } catch (error) {
      logger.error(`💥 Failed to update channel ${id}:`, error)
    }
  }

  // При регистрации устройства — добавить его каналы
  function setBaseChannels(deviceId: number, base: Channel[]) {
    channels.value = channels.value.filter(c => c.device_id !== deviceId)
    channels.value = [...channels.value, ...base]
    logger.info(`📡 Base channels set for ${deviceId}`, base)
  }

  function setChannels(event: DeviceStateEvent) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === event.unit_id)
    if (!device) {
      logger.warn(`⚠️ Device with unit_id=${event.unit_id} not found`)
      return
    }

    const deviceId = device.id
    let updated = [...channels.value]

    switch (event.mode) {
      case StateMode.STATE_SINGLE_BIT: {
        const { ch, value } = event.payload
        updated = updated.map(c =>
          c.device_id === deviceId && c.index === ch
            ? { ...c, state: !!value }
            : c
        )
        break
      }

      case StateMode.STATE_ALL_BIT: {
        const { bitmask } = event.payload
        updated = updated.map(c =>
          c.device_id === deviceId
            ? { ...c, state: (bitmask >> c.index) & 1 ? true : false }
            : c
        )
        break
      }

      case StateMode.STATE_SINGLE_FLOAT: {
        const { ch, value } = event.payload
        updated = updated.map(c =>
          c.device_id === deviceId && c.index === ch
            ? { ...c, state: value }
            : c
        )
        break
      }
    }

    channels.value = updated
  }

  // RESP обработка
  function setResponse(resp: DeviceRespEvent) {
    responses.value[resp.unit_id] = resp
    if (resp.status === 'OK') {
      logger.info(`✅ Command ack from ${resp.unit_id}, packet=${resp.packet_id}`)
    } else {
      logger.warn(
        `⚠️ Command resp from ${resp.unit_id}, packet=${resp.packet_id}, status=${resp.status}, error=${resp.error}`
      )
    }
  }

  // Запросить состояния с устройства
  function requestStates(deviceId: number, deviceType: string) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.id === deviceId)
    if (!device) {
      logger.error(`❌ Device ${deviceId} not found for requestStates`)
      return
    }

    const ws = useWebSocketStore()
    const msg: RequestStateMessage = {
      action: WSAction.GET_STATES,
      unit_id: device.unit_id, // резолвим unit_id
      mode:
        deviceType.toLowerCase() === "ao"
          ? ReqStateMode.REQ_ALL_FLOAT
          : ReqStateMode.REQ_ALL_BIT,
    }
    ws.send(msg)
    logger.info(`📨 Requested states from ${device.unit_id} (${deviceType})`)
  }

  // ---- Команды ----
  function sendDoCommand(unitId: string, ch: number, state: boolean) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      logger.error(`❌ Device ${unitId} not found for DO command`)
      return
    }

    const ws = useWebSocketStore()
    const msg: SetDoCommandMessage = {
      action: WSAction.SET_DO_COMMAND,
      unit_id: device.unit_id,
      mode: CmdMode.SET_SINGLE_BIT,
      ch,
      value: state ? 1 : 0,
    }
    ws.send(msg)
    logger.info(`➡️ DO cmd ${device.unit_id} ch=${ch} → ${state}`)
  }

  function sendDoPairCommand(unitId: string, chA: number, chB: number, state2b: 0|1|2|3) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      logger.error(`❌ Device ${unitId} not found for DO pair command`)
      return
    }

    const ws = useWebSocketStore()
    const msg: SetDoCommandMessage = {
      action: WSAction.SET_DO_COMMAND,
      unit_id: device.unit_id,
      mode: CmdMode.SET_PAIR_BIT,
      chA,
      chB,
      state2b,
    }
    ws.send(msg)
    logger.info(`➡️ DO pair cmd ${device.unit_id} [${chA}/${chB}] → state2b=${state2b}`)
  }

  function sendAoCommand(unitId: string, ch: number, value: number) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      logger.error(`❌ Device ${unitId} not found for AO command`)
      return
    }

    const ws = useWebSocketStore()
    const msg: SetAoCommandMessage = {
      action: WSAction.SET_AO_COMMAND,
      unit_id: device.unit_id,
      ch,
      value,
    }
    ws.send(msg)
    logger.info(`➡️ AO cmd ${device.unit_id} ch=${ch} → ${value}`)
  }

  function resolveUnitId(deviceId: number): string {
    const deviceStore = useDeviceStore()
    const dev = deviceStore.devices.find(d => d.id === deviceId)
    return dev?.unit_id ?? `dev#${deviceId}`
  }

  function resolveUnitName(deviceId: number): string {
    const deviceStore = useDeviceStore()
    const dev = deviceStore.devices.find(d => d.id === deviceId)
    return dev?.name ?? dev?.unit_id ?? `dev#${deviceId}`
  }

  function resolveChannelLabel(ch: Channel): string {
    // 1. TODO: если будет signal_list → ch.signal?.hmi
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
    resolveChannelLabel,
    resolveChannelFullLabel,
    resolveUnitId,
    resolveUnitName,
    channelsByDevice,
    updateChannelField,
    requestStates,
    setBaseChannels,
    setChannels,
    setResponse,
    sendDoCommand,
    sendDoPairCommand,
    sendAoCommand,
  }
})
