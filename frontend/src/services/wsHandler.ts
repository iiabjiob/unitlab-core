import { WSChannel } from '@/types/ws/events'
import { useDeviceStore } from '@/stores/deviceStore'
import { useChannelStore } from '@/stores/channelStore'
import { getLogger } from '@/utils/logger'
import { useSequenceStore } from '@/stores/sequenceStore'
import { useSystemHealthStore } from '@/stores/systemHealthStore'
import { useRealtimeScopeStore } from '@/stores/realtimeScopeStore'

const logger = getLogger('ws')

import type {
  WSEvent,
  DeviceRegisterEvent,
  DeviceHeartbeatEvent,
  DeviceStateEvent,
  DeviceRespEvent,
  SequenceWsEvent,
  ChannelWSEvent,
  SystemHealthChangedEvent,
} from '@/types/ws/events'

export function handleWsEvent(event: WSEvent) {
  const deviceStore = useDeviceStore()
  const channelStore = useChannelStore()
  const sequenceStore = useSequenceStore()
  const systemHealthStore = useSystemHealthStore()
  const realtimeScopeStore = useRealtimeScopeStore()

  // Route sequence events into the sequence store so realtime progress stays in sync.
  if ('topic' in event && (event as SequenceWsEvent).topic === 'sequence') {
    sequenceStore.handleSequenceEvent(event as SequenceWsEvent)
    return
  }

  const channelEvent = event as ChannelWSEvent

  switch (channelEvent.channel) {

    case WSChannel.SYSTEM_INFO: {
      const sysEvent = channelEvent as SystemHealthChangedEvent
      if (sysEvent.event === "system_health_changed") {
        logger.debug("📡 IN ← SYSTEM_HEALTH:", sysEvent)
        systemHealthStore.applySnapshot(sysEvent.snapshot)
        break
      }
      logger.warn("⚠️ Unknown SYSTEM_INFO payload", sysEvent)
      break
    }

    // Device registration: hydrate or merge the newly discovered unit.
    case WSChannel.DEVICE_REGISTER:{
      const devEvent = channelEvent as DeviceRegisterEvent
      logger.debug("📡 IN ← DEVICE_REGISTER:", devEvent)
      const parsedId = Number(devEvent.id)
      const fallbackId = Number(deviceStore.devices.find(item => item.unit_id === devEvent.unit_id)?.id)
      const deviceId = Number.isFinite(parsedId)
        ? parsedId
        : (Number.isFinite(fallbackId) ? fallbackId : devEvent.id)

      // Update or insert the device record first.
      deviceStore.upsertDevice({
        id: deviceId,
        unit_id: devEvent.unit_id,
        device_type: devEvent.device_type,
        num_channels: devEvent.num_channels ?? null,
        firmware_version: devEvent.firmware_version ?? null,
        name: devEvent.name ?? null,
        status: devEvent.status,
        last_seen: devEvent.last_seen ?? null,
        registered_at: devEvent.registered_at ?? null,
        channels: null,
      })

      // Hydrate channel catalog only once for currently observed units.
      if (
        devEvent.channels &&
        realtimeScopeStore.shouldProcessRealtimeForUnit(devEvent.unit_id) &&
        Number.isFinite(Number(deviceId)) &&
        !channelStore.hasDeviceChannels(Number(deviceId))
      ) {
        channelStore.setBaseChannels(Number(deviceId), devEvent.channels)
      }
      break
    }
    // Heartbeat status updates keep online/offline indicators responsive.
    case WSChannel.DEVICE_STATUS:{
      logger.debug("📡 IN ← DEVICE_STATUS:", channelEvent)
      deviceStore.updateStatus(channelEvent as DeviceHeartbeatEvent)
      break
    }
    // Device state broadcasts carry DI/DO/AO changes for visualization.
    case WSChannel.DEVICE_STATE:{
      if (
        !realtimeScopeStore.shouldProcessRealtimeForUnit(channelEvent.unit_id) &&
        !channelStore.hasPendingCommandForUnit(channelEvent.unit_id)
      ) {
        break
      }
      logger.debug("📡 IN ← DEVICE_STATE:", channelEvent)
      channelStore.setChannels(channelEvent as DeviceStateEvent)
      break
    }
    // Command responses confirm that the device acknowledged the instruction.
    case WSChannel.DEVICE_RESP: {
      logger.debug("📡 IN ← DEVICE_RESP:", channelEvent)
      channelStore.setResponse(channelEvent as DeviceRespEvent)
      break
    }
    default:
      logger.warn("⚠️ Unknown WS channel:", channelEvent)
  }
}
