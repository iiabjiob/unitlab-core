import { useWebSocketStore } from "@/stores/websocketStore"
import { WSChannel } from "@/types/ws/events"

export function initWebSocket() {
  const wsStore = useWebSocketStore()

  wsStore.connect()

  wsStore.requestServerSubscribe([
    WSChannel.DEVICE_REGISTER,
    WSChannel.DEVICE_STATUS,
    WSChannel.DEVICE_STATE,
    WSChannel.DEVICE_RESP,
    WSChannel.TIME_STATUS,
    WSChannel.EVENT_LOG,
  ])

}
