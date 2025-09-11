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
import { getLogger } from '@/utils/logger'

const logger = getLogger('CH')

export const useChannelStore = defineStore('channelStore', () => {
  // Все каналы: unit_id → Channel[]
  const channels = ref<Record<string, Channel[]>>({})

  // Последние RESP от устройств
  const responses = ref<Record<string, DeviceRespEvent>>({})

  function setBaseChannels(unitId: string, base: Channel[]) {
    // сохраняем каналы без состояния, только структура (index, name, type)
    channels.value[unitId] = base.map(ch => ({ ...ch }))
    logger.info(`📡 Base channels set for ${unitId}`, channels.value[unitId])
  }
  // Обновление от backend (DeviceStateEvent → Channels)
  function setChannels(event: DeviceStateEvent) {
    const unitId = event.unit_id
    // базовые каналы уже есть после REGISTER
    const prev = channels.value[unitId] ?? []
    let updated = [...prev]

    switch (event.mode) {
      case StateMode.STATE_SINGLE_BIT: {
        const { ch, value } = event.payload
        const idx = updated.findIndex(c => c.index === ch)
        if (idx !== -1) {
          updated[idx] = { ...updated[idx], state: !!value }
        }
        break
      }

      case StateMode.STATE_ALL_BIT: {
        const { bitmask } = event.payload
        updated = updated.map(c => ({
          ...c,
          state: (bitmask >> c.index) & 1 ? true : false,
        }))
        break
      }

      case StateMode.STATE_SINGLE_FLOAT: {
        const { ch, value } = event.payload
        const idx = updated.findIndex(c => c.index === ch)
        if (idx !== -1) {
          updated[idx] = { ...updated[idx], state: value }
        }
        break
      }
    }

    channels.value[unitId] = updated
    logger.debug(`📡 Updated channels for ${unitId}`, channels.value[unitId])
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
    setBaseChannels,
    setChannels,
    setResponse,
    sendDoCommand,
    sendDoPairCommand,
    sendAoCommand,
  }
})
