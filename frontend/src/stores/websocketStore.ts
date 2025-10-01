import { defineStore } from "pinia"
import { ref } from "vue"
import { handleWsEvent } from "@/services/wsHandler"
import type { WSEvent } from "@/types/ws/events"
import { WSAction, type WSMessage } from "@/types/ws/messages"
import { getLogger } from "@/utils/logger"

const logger = getLogger("WS")

export const useWebSocketStore = defineStore("websocketStore", () => {
  const socket = ref<WebSocket | null>(null)
  const isConnected = ref(false)
  const everConnected = ref(false)

  // очередь всех сообщений, пока сокет не открыт
  const messageQueue: WSMessage[] = []

  // reconnect backoff
  const reconnectAttempts = ref(0)
  const maxReconnectAttempts = 5
  const reconnectDelay = 1000
  const maxReconnectDelay = 30000

  // ---------------- connect ----------------
  function connect() {
    if (
      socket.value &&
      (socket.value.readyState === WebSocket.OPEN ||
        socket.value.readyState === WebSocket.CONNECTING)
    ) {
      return
    }

    const wsProtocol = window.location.protocol === "https:" ? "wss" : "ws"
    const wsUrl = `${wsProtocol}://${window.location.host}/ws/ws`

    logger.info("🔌 Connecting")
    socket.value = new WebSocket(wsUrl)

    socket.value.onopen = () => {
      isConnected.value = true
      reconnectAttempts.value = 0
      everConnected.value = true // было соединение хоть раз
      logger.info("✅ Connected")

      // Flush queued messages first
      while (messageQueue.length > 0) {
        const msg = messageQueue.shift()!
        _sendNow(msg)
      }
    }

    socket.value.onclose = (event: CloseEvent) => {
      logger.warn("💥 Disconnected", event)
      isConnected.value = false
      socket.value = null
      _scheduleReconnect()
    }

    socket.value.onmessage = (event: MessageEvent) => {
      const data: WSEvent = JSON.parse(event.data)

      // dispatch to domain stores
      handleWsEvent(data)
    }

    socket.value.onerror = (error: Event) => {
      logger.error("⚠️ Error", error)
      socket.value?.close()
    }
  }

  // ---------------- send with queue ----------------
  function _sendNow(message: WSMessage) {
    if (socket.value && socket.value.readyState === WebSocket.OPEN) {
      socket.value.send(JSON.stringify(message))
      logger.debug("📤 Sent", message)
    } else {
      logger.warn("💤 Tried to send but socket not open", message)
    }
  }

  function send(message: WSMessage) {
    if (isConnected.value) {
      _sendNow(message)
    } else {
      messageQueue.push(message)
      logger.info("💤 Message queued", message)
    }
  }

  // ---------------- reconnect backoff ----------------
  function _scheduleReconnect() {
    let delay: number
    if (reconnectAttempts.value < maxReconnectAttempts) {
      reconnectAttempts.value++
      delay = reconnectDelay * Math.pow(2, reconnectAttempts.value - 1)
    } else {
      delay = maxReconnectDelay
    }
    const seconds = (delay / 1000).toFixed(1)
    logger.info(`⏳ Reconnect in ${seconds}s (attempt #${reconnectAttempts.value})`)
    setTimeout(() => {
      logger.info(`🔄 Reconnect attempt #${reconnectAttempts.value}`)
      connect()
    }, delay)
  }

  return {
    socket,
    isConnected,
    everConnected,
    connect,
    send,
  }
})
