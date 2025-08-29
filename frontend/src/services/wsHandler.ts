import { WSChannel } from '@/types/ws/events'
import { useDeviceStore } from '@/stores/deviceStore'
import { useChannelStore } from '@/stores/channelStore'
import { useTimeStore } from '@/stores/timeStore'

import type {
  WSEvent,
  DeviceRegisterEvent,
  DeviceHeartbeatEvent,
  DeviceStateEvent,
  DeviceRespEvent,
  TimeStatusEvent } from '@/types/ws/events'

export function handleWsEvent(event: WSEvent) {
  const deviceStore = useDeviceStore()
  const channelStore = useChannelStore()
  const timeStore = useTimeStore()

  switch (event.channel) {

    // --- регистрация устройства ---
    case WSChannel.DEVICE_REGISTER:
      deviceStore.upsertDevice(event as DeviceRegisterEvent)
      break

    // --- статус (онлайн/оффлайн) ---
    case WSChannel.DEVICE_STATUS:
      deviceStore.updateStatus(event as DeviceHeartbeatEvent)
      break

    // --- состояние сигналов ---
    case WSChannel.DEVICE_STATE:
      channelStore.setSignals(event as DeviceStateEvent)
      break

    // --- ответы на команды ---
    case WSChannel.DEVICE_RESP: {
      channelStore.setResponse(event as DeviceRespEvent)
      break
    }

    // --- время ---
    case WSChannel.TIME_STATUS:
      timeStore.updateFromSync(event as TimeStatusEvent)
      break

  }
}
