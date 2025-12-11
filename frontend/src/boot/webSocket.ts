// src/boot/websocket.ts
import { useWebSocketStore } from "@/stores/websocketStore"
import { logger } from "@/utils/logger"

export function bootWebSocket() {
  const ws = useWebSocketStore()

  ws.connect()

  logger.info("🔌 Boot: WebSocket connected")
}
