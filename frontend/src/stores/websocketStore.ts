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
  const RECONNECT_NUDGE_DEBOUNCE_MS = 1500
  let lastReconnectNudgeAt = 0

  const wsProtocol = location.protocol === "https:" ? "wss" : "ws"
  const configuredWsUrl = String(import.meta.env.VITE_WS_URL ?? "").trim()
  const configuredFallbackWsUrl = String(import.meta.env.VITE_WS_FALLBACK_URL ?? "").trim()
  const proxiedWsUrl = `${wsProtocol}://${location.host}/ws/ws`
  const directDevWsUrl = `${wsProtocol}://localhost:8000/ws/ws`

  const envFallbackCandidates = configuredFallbackWsUrl
    .split(",")
    .map(url => url.trim())
    .filter(url => url.length > 0)

  const wsCandidates = Array.from(
    new Set([
      configuredWsUrl,
      ...envFallbackCandidates,
      proxiedWsUrl,
      ...(import.meta.env.DEV ? [directDevWsUrl] : []),
    ].filter(url => Boolean(url))),
  )
  let wsCandidateIndex = 0

  // reconnect backoff
  const reconnectAttempts = ref(0)
  const maxReconnectAttempts = 100
  const reconnectDelay = 1000
  const maxReconnectDelay = 30000

  function clearQueue() {
    messageQueue.length = 0
  }

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

  function currentWsUrl(): string {
    if (wsCandidates.length === 0) {
      return proxiedWsUrl
    }
    const normalizedIndex = ((wsCandidateIndex % wsCandidates.length) + wsCandidates.length) % wsCandidates.length
    return wsCandidates[normalizedIndex]
  }

  function rotateWsCandidate(reason: string) {
    if (wsCandidates.length <= 1) return
    wsCandidateIndex = (wsCandidateIndex + 1) % wsCandidates.length
    logger.warn(`🔀 Switching WS endpoint after ${reason} → ${currentWsUrl()}`)
  }

  function recycleSocketAfterFailure(ws: WebSocket, reason: string) {
    if (socket.value !== ws) return

    clearConnectTimeout()
    logger.warn(`♻️ Recycling socket after ${reason}`)
    rotateWsCandidate(reason)

    intentionallyClosedSocket = ws
    try {
      ws.close()
    } catch {
      // ignore close errors, we still transition state and reconnect
    }

    socket.value = null
    isConnected.value = false
    isConnecting.value = false

    if (!manualDisconnect) {
      scheduleReconnect()
    }
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

    const wsUrl = currentWsUrl()

    logger.info(`🔌 Connecting WS → ${wsUrl}`)

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
      wsCandidateIndex = 0

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

      if (!isIntentional) {
        rotateWsCandidate("close")
        scheduleReconnect()
      }
    }

    ws.onerror = (err) => {
      if (socket.value !== ws) return
      logger.error("⚠️ WebSocket Error", err)
      recycleSocketAfterFailure(ws, "error")
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
        recycleSocketAfterFailure(ws, "connect-timeout")
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
    if (socket.value?.readyState === WebSocket.OPEN) {
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

    reconnectAttempts.value = Math.min(reconnectAttempts.value + 1, maxReconnectAttempts)

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

  function nudgeReconnect(reason: "online" | "visible" | "focus" | "pageshow") {
    if (manualDisconnect) return
    if (isConnected.value) return
    if (typeof navigator !== "undefined" && navigator.onLine === false) {
      logger.info(`🌐 Skip reconnect nudge (${reason}): browser is offline`)
      return
    }

    const now = Date.now()
    if (now - lastReconnectNudgeAt < RECONNECT_NUDGE_DEBOUNCE_MS) {
      logger.debug(`🌐 Skip reconnect nudge (${reason}): debounced`)
      return
    }
    lastReconnectNudgeAt = now

    logger.info(`🌐 Reconnect nudge (${reason}): retry now`)
    reconnectAttempts.value = 0
    clearReconnectTimer()
    clearConnectTimeout()
    connect()
  }

  /* ---------------- MANUAL CLOSE ---------------- */
  function disconnect() {
    manualDisconnect = true
    clearReconnectTimer()
    clearConnectTimeout()
    clearQueue()

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
    nudgeReconnect,
    disconnect,
    send,
  }
})
