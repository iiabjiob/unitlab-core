// src/wsInit.ts
import { useWebSocketStore } from "@/stores/websocketStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useTimeStore } from "@/stores/timeStore"
import { WSChannel } from "@/types/ws/events"
import type {
  DeviceRegisterEvent,
  DeviceHeartbeatEvent,
  DeviceStateEvent,
  TimeStatusEvent
} from "@/types/ws/events"

export function initWebSocket() {
  const wsStore = useWebSocketStore()
  const deviceStore = useDeviceStore()
  const channelStore = useChannelStore()
  const timeStore = useTimeStore()

  // Поднять соединение
  wsStore.connect()

  // --- Global listeners ---
  wsStore.onChannel(WSChannel.DEVICE_REGISTER, (event: DeviceRegisterEvent) => {
    deviceStore.upsertDevice(event)
  })

  wsStore.onChannel(WSChannel.DEVICE_STATUS, (event: DeviceHeartbeatEvent) => {
    deviceStore.updateStatus(event)
  })

  wsStore.onChannel(WSChannel.DEVICE_STATE, (event: DeviceStateEvent) => {
    channelStore.setSignals(event)
  })

  wsStore.onChannel(WSChannel.TIME_STATUS, (event: TimeStatusEvent) => {
    timeStore.updateFromSync(event)
  })
}
