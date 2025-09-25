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
import type { EventLogEntry } from '@/types/eventLog'

export function handleWsEvent(event: WSEvent) {
  const deviceStore = useDeviceStore()
  const channelStore = useChannelStore()
  const timeStore = useTimeStore()
  const eventLogStore = useEventLogStore();

  switch (event.channel) {

    // --- регистрация устройства ---
    case WSChannel.DEVICE_REGISTER:{
      const devEvent = event as DeviceRegisterEvent
      logger.debug("📡 IN ← DEVICE_REGISTER:", devEvent)

      // 1. обновляем устройства
      deviceStore.upsertDevice(devEvent)

      // 2. обновляем каналы (если они пришли в событии)
      if (devEvent.channels) {
        channelStore.setBaseChannels(devEvent.id, devEvent.channels)
      }
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
      channelStore.setChannels(event as DeviceStateEvent)
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

      // маппим WS → Store
      const entry: EventLogEntry = {
        id: e.id,
        ts: e.ts,
        dir: e.dir,
        source: e.source,
        channelOrAction: e.channelOrAction,
        unitId: e.unitId,
        type: e.type,
        summary: e.summary,
        payload: e.payload,
        createdAt: new Date(e.ts).toISOString(), // добавляем недостающий required
      }

      eventLogStore.add(entry)
      break
    }
  }
}
