import { WSChannel } from '@/types/ws/events'
import { useDeviceStore } from '@/stores/deviceStore'
import { useChannelStore } from '@/stores/channelStore'
import { useTimeStore } from '@/stores/timeStore'
import { useEventLogStore } from '@/stores/eventLogStore'
import { getLogger } from '@/utils/logger'
import { useSequenceStore } from '@/stores/sequenceStore'

const logger = getLogger('ws')

import type {
  WSEvent,
  DeviceRegisterEvent,
  DeviceHeartbeatEvent,
  DeviceStateEvent,
  DeviceRespEvent,
  TimeStatusEvent,
  WSEventLogEvent,
  SequenceWsEvent,
  ChannelWSEvent,
} from '@/types/ws/events'

export function handleWsEvent(event: WSEvent) {
  const deviceStore = useDeviceStore()
  const channelStore = useChannelStore()
  const timeStore = useTimeStore()
  const eventLogStore = useEventLogStore();
  const sequenceStore = useSequenceStore()

  if ('topic' in event && (event as SequenceWsEvent).topic === 'sequence') {
    sequenceStore.handleSequenceEvent(event as SequenceWsEvent)
    return
  }

  const channelEvent = event as ChannelWSEvent

  switch (channelEvent.channel) {

    // --- регистрация устройства ---
    case WSChannel.DEVICE_REGISTER:{
      const devEvent = channelEvent as DeviceRegisterEvent
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
      logger.debug("📡 IN ← DEVICE_STATUS:", channelEvent)
      deviceStore.updateStatus(channelEvent as DeviceHeartbeatEvent)
      break
    }
    // --- состояние сигналов ---
    case WSChannel.DEVICE_STATE:{
      logger.debug("📡 IN ← DEVICE_STATE:", channelEvent)
      channelStore.setChannels(channelEvent as DeviceStateEvent)
      break
    }
    // --- ответы на команды ---
    case WSChannel.DEVICE_RESP: {
      logger.debug("📡 IN ← DEVICE_RESP:", channelEvent)
      channelStore.setResponse(channelEvent as DeviceRespEvent)
      break
    }
    // --- время ---
    case WSChannel.TIME_STATUS:{
      logger.debug("📡 IN ← TIME_STATUS:", channelEvent)
      timeStore.updateFromSync(channelEvent as TimeStatusEvent)
      break
    }
    // --- События ---
    case WSChannel.EVENT_LOG: {
      eventLogStore.handleWs(channelEvent as WSEventLogEvent)
      break
    }
  }
}
