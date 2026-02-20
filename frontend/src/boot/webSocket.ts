// src/boot/websocket.ts
import { useWebSocketStore } from "@/stores/websocketStore"
import { logger } from "@/utils/logger"

export function bootWebSocket() {
  const ws = useWebSocketStore()

  ws.connect()

  window.addEventListener("online", () => ws.nudgeReconnect("online"))
  document.addEventListener("visibilitychange", () => {
    if (document.visibilityState === "visible") {
      ws.nudgeReconnect("visible")
    }
  })
  window.addEventListener("focus", () => ws.nudgeReconnect("focus"))
  window.addEventListener("pageshow", () => ws.nudgeReconnect("pageshow"))

  logger.info("🔌 Boot: WebSocket connect initiated (+ wake reconnect hooks)")
}
