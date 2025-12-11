import { defineStore } from "pinia"
import { ref } from "vue"
import { handleWsEvent } from "@/services/wsHandler"
import type { WSEvent } from "@/types/ws/events"
import type { WSMessage } from "@/types/ws/messages"
import { getLogger } from "@/utils/logger"

const logger = getLogger("WS")

export const useWebSocketStore = defineStore("websocketStore", () => {
  const socket = ref<WebSocket | null>(null)
  const isConnected = ref(false)
  const everConnected = ref(false)

  let manualClose = false

  const messageQueue: WSMessage[] = []
  const MAX_QUEUE = 2000

  // reconnect backoff
  const reconnectAttempts = ref(0)
  const reconnectDelay = 1000
  const maxReconnectDelay = 30000

  /* ---------------- CONNECT ---------------- */
  function connect() {
    manualClose = false

    if (socket.value) {
      logger.debug("♻️ Closing previous socket before reconnect")
      socket.value.close()
      socket.value = null
    }

    const wsProtocol = location.protocol === "https:" ? "wss" : "ws"
    const wsUrl = `${wsProtocol}://${location.host}/ws/ws`

    socket.value = new WebSocket(wsUrl)

    socket.value.onopen = () => {
      isConnected.value = true
      everConnected.value = true
      reconnectAttempts.value = 0

      logger.info("✅ Connected")

      // flush queue
      while (messageQueue.length > 0) {
        _sendNow(messageQueue.shift()!)
      }
    }

    socket.value.onclose = (ev) => {
      logger.warn("💥 Disconnected", ev)
      isConnected.value = false
      socket.value = null

      if (!manualClose) scheduleReconnect()
    }

    socket.value.onerror = (err) => {
      logger.error("⚠️ WebSocket Error", err)
      socket.value?.close()
    }

    socket.value.onmessage = (ev) => {
      const msg: WSEvent = JSON.parse(ev.data)
      handleWsEvent(msg)
    }
  }

  /* ---------------- SEND ---------------- */
  function _sendNow(msg: WSMessage) {
    if (socket.value?.readyState === WebSocket.OPEN) {
      socket.value.send(JSON.stringify(msg))
      logger.debug("📤 Sent", msg)
    } else {
      logger.warn("💤 Socket not open", msg)
    }
  }

  function send(msg: WSMessage) {
    if (isConnected.value) {
      _sendNow(msg)
    } else {
      if (messageQueue.length > MAX_QUEUE) {
        logger.warn("⚠️ Queue full, dropping oldest")
        messageQueue.shift()
      }
      messageQueue.push(msg)
      logger.info("🕒 Queued", msg)
    }
  }

  /* ---------------- RECONNECT ---------------- */
  function scheduleReconnect() {
    reconnectAttempts.value++

    const delay = Math.min(
      reconnectDelay * Math.pow(2, reconnectAttempts.value - 1),
      maxReconnectDelay
    )

    logger.info(`⏳ Reconnect in ${(delay / 1000).toFixed(1)}s (#${reconnectAttempts.value})`)

    setTimeout(() => {
      logger.info("🔄 Reconnecting…")
      connect()
    }, delay)
  }

  /* ---------------- MANUAL CLOSE ---------------- */
  function disconnect() {
    manualClose = true
    socket.value?.close()
    socket.value = null
    isConnected.value = false
  }

  return {
    socket,
    isConnected,
    everConnected,
    connect,
    disconnect,
    send,
  }
})
