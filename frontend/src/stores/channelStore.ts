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
  const prev = channels.value[unitId] ?? []
  let updated = [...prev]

  switch (event.mode) {
    case StateMode.STATE_SINGLE_BIT: {
      const { ch, value } = event.payload
      const idx = updated.findIndex(c => c.index === ch)
      if (idx !== -1) {
        updated[idx].state = !!value
      } else {
        updated.push({
          index: ch,
          device_id: unitId,
          state: !!value,
          name: `CH${ch + 1}`,
        })
      }
      break
    }

    case StateMode.STATE_ALL_BIT: {
      const { bitmask } = event.payload
      const total = (useDeviceStore().devices.find(d => d.unit_id === unitId)?.channels) ?? 0
      updated = []
      for (let i = 0; i < total; i++) {
        updated.push({
          index: i,
          device_id: unitId,
          state: (bitmask >> i) & 1 ? true : false,
          name: `CH${i + 1}`,
        })
      }
      break
    }

    case StateMode.STATE_SINGLE_FLOAT: {
      const { ch, value } = event.payload
      const idx = updated.findIndex(c => c.index === ch)
      if (idx !== -1) {
        updated[idx].state = value
      } else {
        updated.push({
          index: ch,
          device_id: unitId,
          state: value,
          name: `AO${ch + 1}`,
        })
      }
      break
    }
  }

  channels.value[unitId] = updated
  logger.debug(`📡 Updated channels for ${unitId}`, updated)
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

  function sendDoPairCommand(unitId: string, chA: number, chB: number, state2b: 0|1|2|3) {
    const ws = useWebSocketStore()
    const msg: SetDoCommandMessage = {
      action: WSAction.SET_DO_COMMAND,
      unit_id: unitId,
      mode: CmdMode.SET_PAIR_BIT, // <- atomic pair
      chA,
      chB,
      state2b,
    }
    ws.send(msg)
    logger.info(`➡️ DO pair cmd ${unitId} [${chA}/${chB}] → state2b=${state2b}`)
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
    sendDoPairCommand,
    sendAoCommand,
  }
})
