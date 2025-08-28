import { WSChannel } from '@/types/ws/events'
import type { WSEvent, DeviceRegisterEvent, DeviceHeartbeatEvent, TimeStatusEvent } from '@/types/ws/events'
import { useDeviceStore } from '@/stores/deviceStore'
import { useTimeStore } from '@/stores/timeStore'

export function handleWsEvent(event: WSEvent) {
  const deviceStore = useDeviceStore()
  const timeStore = useTimeStore()

  switch (event.channel) {
    case WSChannel.DEVICE_REGISTER:
      deviceStore.upsertDevice(event as DeviceRegisterEvent)
      break

    case WSChannel.DEVICE_STATUS:
      deviceStore.updateStatus(event as DeviceHeartbeatEvent)
      break

    case WSChannel.TIME_STATUS:
      timeStore.updateFromSync(event as TimeStatusEvent)
      break

  }
}
