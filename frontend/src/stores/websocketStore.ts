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
  const hasStarted = ref(false)
  const isConnecting = ref(false)

  let manualDisconnect = false
  let intentionallyClosedSocket: WebSocket | null = null
  let connectTimeout: ReturnType<typeof setTimeout> | null = null
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null

  const messageQueue: WSMessage[] = []
  const MAX_QUEUE = 2000
  const CONNECT_TIMEOUT_MS = 5000

  // reconnect backoff
  const reconnectAttempts = ref(0)
  const reconnectDelay = 1000
  const maxReconnectDelay = 30000

  function clearConnectTimeout() {
    if (!connectTimeout) return
    clearTimeout(connectTimeout)
    connectTimeout = null
  }

  function clearReconnectTimer() {
    if (!reconnectTimer) return
    clearTimeout(reconnectTimer)
    reconnectTimer = null
  }

  /* ---------------- CONNECT ---------------- */
  function connect() {
    const activeSocket = socket.value
    if (
      activeSocket &&
      (
        activeSocket.readyState === WebSocket.OPEN ||
        activeSocket.readyState === WebSocket.CONNECTING
      )
    ) {
      logger.debug("🔒 Skip connect(): socket already open/connecting")
      return
    }

    hasStarted.value = true
    isConnecting.value = true
    manualDisconnect = false
    clearReconnectTimer()

    if (activeSocket) {
      logger.debug("♻️ Closing previous socket before reconnect")
      intentionallyClosedSocket = activeSocket
      activeSocket.close()
      socket.value = null
    }
    clearConnectTimeout()

    const wsProtocol = location.protocol === "https:" ? "wss" : "ws"
    const wsUrl = `${wsProtocol}://${location.host}/ws/ws`

    const ws = new WebSocket(wsUrl)
    socket.value = ws

    ws.onopen = () => {
      if (socket.value !== ws) return
      clearConnectTimeout()
      clearReconnectTimer()
      isConnected.value = true
      isConnecting.value = false
      everConnected.value = true
      reconnectAttempts.value = 0

      logger.info(`✅ Connected (${wsUrl})`)

      // flush queue
      while (messageQueue.length > 0) {
        _sendNow(messageQueue.shift()!)
      }
    }

    ws.onclose = (ev) => {
      const isIntentional = intentionallyClosedSocket === ws || manualDisconnect
      if (intentionallyClosedSocket === ws) {
        intentionallyClosedSocket = null
      }
      if (socket.value !== ws) {
        return
      }

      clearConnectTimeout()
      logger.warn(
        `💥 Disconnected code=${ev.code} reason="${ev.reason || "n/a"}" clean=${ev.wasClean}`,
        ev,
      )
      isConnected.value = false
      isConnecting.value = false
      socket.value = null

      if (!isIntentional) scheduleReconnect()
    }

    ws.onerror = (err) => {
      if (socket.value !== ws) return
      logger.error("⚠️ WebSocket Error", err)
      ws.close()
    }

    ws.onmessage = (ev) => {
      if (socket.value !== ws) return
      try {
        const msg: WSEvent = JSON.parse(ev.data)
        handleWsEvent(msg)
      } catch (error) {
        logger.error("⚠️ Failed to parse WS message", error)
      }
    }

    connectTimeout = setTimeout(() => {
      if (socket.value !== ws) return
      if (ws.readyState === WebSocket.CONNECTING) {
        logger.warn(`⏱️ Connect timeout after ${CONNECT_TIMEOUT_MS}ms, restarting socket`)
        ws.close()
      }
    }, CONNECT_TIMEOUT_MS)
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
    if (manualDisconnect) return
    if (reconnectTimer) return

    reconnectAttempts.value++

    const delay = Math.min(
      reconnectDelay * Math.pow(2, reconnectAttempts.value - 1),
      maxReconnectDelay
    )

    logger.info(`⏳ Reconnect in ${(delay / 1000).toFixed(1)}s (#${reconnectAttempts.value})`)

    reconnectTimer = setTimeout(() => {
      reconnectTimer = null
      if (manualDisconnect) return
      logger.info("🔄 Reconnecting…")
      connect()
    }, delay)
  }

  /* ---------------- MANUAL CLOSE ---------------- */
  function disconnect() {
    manualDisconnect = true
    clearReconnectTimer()
    clearConnectTimeout()

    if (socket.value) {
      intentionallyClosedSocket = socket.value
      socket.value.close()
    }

    socket.value = null
    isConnected.value = false
    isConnecting.value = false
  }

  return {
    socket,
    isConnected,
    everConnected,
    hasStarted,
    isConnecting,
    reconnectAttempts,
    connect,
    disconnect,
    send,
  }
})
