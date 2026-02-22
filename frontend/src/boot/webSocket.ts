// src/boot/websocket.ts
import { useWebSocketStore } from "@/stores/websocketStore"
import { logger } from "@/utils/logger"
import { devPerfIncrement, devPerfMeasureStart } from "@/utils/devPerf"

let reconnectWakeHooksInstalled = false

function handleOnlineReconnectNudge() {
  useWebSocketStore().nudgeReconnect("online")
}

function handleVisibilityReconnectNudge() {
  if (document.visibilityState !== "visible") {
    return
  }
  useWebSocketStore().nudgeReconnect("visible")
}

function handleFocusReconnectNudge() {
  useWebSocketStore().nudgeReconnect("focus")
}

function handlePageShowReconnectNudge() {
  useWebSocketStore().nudgeReconnect("pageshow")
}

export function bootWebSocket() {
  devPerfIncrement("bootWebSocket.calls")
  const endBootWebSocketMeasure = devPerfMeasureStart("boot.websocket")
  const ws = useWebSocketStore()

  ws.connect()

  if (!reconnectWakeHooksInstalled) {
    reconnectWakeHooksInstalled = true
    devPerfIncrement("bootWebSocket.wakeHooks.installs")
    window.addEventListener("online", handleOnlineReconnectNudge)
    document.addEventListener("visibilitychange", handleVisibilityReconnectNudge)
    window.addEventListener("focus", handleFocusReconnectNudge)
    window.addEventListener("pageshow", handlePageShowReconnectNudge)
  } else {
    devPerfIncrement("bootWebSocket.wakeHooks.reused")
  }

  logger.info("🔌 Boot: WebSocket connect initiated (+ wake reconnect hooks)")
  endBootWebSocketMeasure({ hooksInstalled: reconnectWakeHooksInstalled })
}
