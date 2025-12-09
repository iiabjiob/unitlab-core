// src/boot/websocket.ts
import { useWebSocketStore } from "@/stores/websocketStore"
import { logger } from "@/utils/logger"

export function bootWebSocket() {
  const ws = useWebSocketStore()

  logger.info("🔌 Boot: initializing WebSocket…")

  ws.connect()
}
