import { WSChannel } from '@/types/ws/events'
import { useDeviceStore } from '@/stores/deviceStore'
import { useChannelStore } from '@/stores/channelStore'
import { useTimeStore } from '@/stores/timeStore'
import { useEventLogStore } from '@/stores/eventLogStore'
import { getLogger } from '@/utils/logger'

const logger = getLogger('ws')

import type {
  WSEvent,
  DeviceRegisterEvent,
  DeviceHeartbeatEvent,
  DeviceStateEvent,
  DeviceRespEvent,
  TimeStatusEvent,
  EventLogEvent
} from '@/types/ws/events'

export function handleWsEvent(event: WSEvent) {
  const deviceStore = useDeviceStore()
  const channelStore = useChannelStore()
  const timeStore = useTimeStore()
  const eventLogStore = useEventLogStore();

  switch (event.channel) {

    // --- регистрация устройства ---
    case WSChannel.DEVICE_REGISTER:{
      logger.debug("📡 IN ← DEVICE_REGISTER:", event)
      deviceStore.upsertDevice(event as DeviceRegisterEvent)
      break
    }
    // --- статус (онлайн/оффлайн) ---
    case WSChannel.DEVICE_STATUS:{
      logger.debug("📡 IN ← DEVICE_STATUS:", event)
      deviceStore.updateStatus(event as DeviceHeartbeatEvent)
      break
    }
    // --- состояние сигналов ---
    case WSChannel.DEVICE_STATE:{
      logger.debug("📡 IN ← DEVICE_STATE:", event)
      channelStore.setSignals(event as DeviceStateEvent)
      break
    }
    // --- ответы на команды ---
    case WSChannel.DEVICE_RESP: {
      logger.debug("📡 IN ← DEVICE_RESP:", event)
      channelStore.setResponse(event as DeviceRespEvent)
      break
    }
    // --- время ---
    case WSChannel.TIME_STATUS:{
      logger.debug("📡 IN ← TIME_STATUS:", event)
      timeStore.updateFromSync(event as TimeStatusEvent)
      break
    }
    // --- События ---
    case WSChannel.EVENT_LOG: {
      const e = event as EventLogEvent
      eventLogStore.add(e)
      break
    }
  }
}
