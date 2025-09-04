import { useWebSocketStore } from "@/stores/websocketStore"

export function initWebSocket() {
  const wsStore = useWebSocketStore()

  wsStore.connect()
}
