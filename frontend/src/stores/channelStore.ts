// src/stores/channelStore.ts
import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { Channel } from '@/types/channel'

import {
  StateMode,
  type DeviceStateEvent,
  type DeviceRespEvent } from '@/types/ws/events'

import {
  WSAction,
  CmdMode,
  ReqStateMode,
  type SetDoCommandMessage,
  type SetAoCommandMessage,
  type RequestStateMessage } from '@/types/ws/messages'

import { useWebSocketStore } from "@/stores/websocketStore"
import { useDeviceStore } from '@/stores/deviceStore'
import { getLogger } from '@/utils/logger'

const logger = getLogger('CH')

export const useChannelStore = defineStore('channelStore', () => {
  // Все каналы: unit_id → Channel[]
  const channels = ref<Record<string, Channel[]>>({})

  // Последние RESP от устройств
  const responses = ref<Record<string, DeviceRespEvent>>({})

  // Обновление от backend (DeviceStateEvent → Channels)
  function setSignals(event: DeviceStateEvent) {
    const unitId = event.unit_id

    // Пример: маппинг из payload в Channel[]
    const mapped: Channel[] = []
    switch (event.mode) {
      case StateMode.STATE_SINGLE_BIT: {
        const { ch, value } = event.payload
        const deviceStore = useDeviceStore()
        const dev = deviceStore.devices.find(d => d.unit_id === unitId)

        const totalChannels = dev?.channels ?? 32
        const deviceType = dev?.device_type?.toUpperCase() ?? event.device_type.toUpperCase()

        if (ch < totalChannels) {
          mapped.push({
            index: ch,
            device_id: unitId,
            type: deviceType === 'DI' ? 'DI' : 'DO',
            state: !!value,
            name: `CH${ch + 1}`,
          })
        }
        break
      }

      case StateMode.STATE_ALL_BIT: {
        const { bitmask } = event.payload
        const deviceStore = useDeviceStore()
        const dev = deviceStore.devices.find(d => d.unit_id === unitId)

        const totalChannels = dev?.channels ?? 32   // берём из БД/стора
        const deviceType = dev?.device_type?.toUpperCase() ?? event.device_type.toUpperCase()

        for (let i = 0; i < totalChannels; i++) {
          mapped.push({
            index: i,
            device_id: unitId,
            type: deviceType === 'DI' ? 'DI' : 'DO',  // тип тоже из БД
            state: (bitmask >> i) & 1 ? true : false,
            name: `CH${i + 1}`,
          })
        }
        break
      }

      case StateMode.STATE_SINGLE_FLOAT: {
        const { ch, value } = event.payload
        const deviceStore = useDeviceStore()
        const dev = deviceStore.devices.find(d => d.unit_id === unitId)

        const totalChannels = dev?.channels ?? 4
        const deviceType = dev?.device_type?.toUpperCase() ?? event.device_type.toUpperCase()

        if (deviceType === 'AO' && ch < totalChannels) {
          mapped.push({
            index: ch,
            device_id: unitId,
            type: 'AO',
            state: value,
            name: `AO${ch + 1}`,
          })
        }
        break
      }
    }

    channels.value[unitId] = mapped
    logger.debug(`📡 Updated channels for ${unitId}`, mapped)
  }

  // ---- RESP обработка ----
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

  function requestStates(unitId: string, deviceType: string) {
    const ws = useWebSocketStore()
    const msg: RequestStateMessage = {
      action: WSAction.GET_STATES,
      unit_id: unitId,
      device_type: deviceType.toLowerCase() as "di" | "do" | "ao",
      mode:
        deviceType.toLowerCase() === "ao"
          ? ReqStateMode.REQ_ALL_FLOAT
          : ReqStateMode.REQ_ALL_BIT,
    }
    ws.send(msg)
    logger.info(`📨 Requested states from ${unitId} (${deviceType})`)
  }

  // ---- Команды ----

  function sendDoCommand(unitId: string, ch: number, state: boolean) {
    const ws = useWebSocketStore()
    const msg: SetDoCommandMessage = {
      action: WSAction.SET_DO_COMMAND,
      unit_id: unitId,
      mode: CmdMode.SET_SINGLE_BIT,
      ch,
      value: state ? 1 : 0,
    }
    ws.send(msg)
    logger.info(`➡️ DO cmd ${unitId} ch=${ch} → ${state}`)
  }

  function sendAoCommand(unitId: string, ch: number, value: number) {
    const ws = useWebSocketStore()
    const msg: SetAoCommandMessage = {
      action: WSAction.SET_AO_COMMAND,
      unit_id: unitId,
      ch,
      value,
    }
    ws.send(msg)
    logger.info(`➡️ AO cmd ${unitId} ch=${ch} → ${value}`)
  }

  return {
    channels,
    responses,
    requestStates,
    setSignals,
    setResponse,
    sendDoCommand,
    sendAoCommand,
  }
})
