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

  // очередь всех сообщений, пока сокет не открыт
  const messageQueue: WSMessage[] = []

  // server subscriptions
  const activeServerSubs = ref<Set<string>>(new Set())
  const pendingServerSubs: string[][] = []
  const serverSyncedSubs = new Set<string>()

  // local listeners
  const localListeners = new Map<string, Set<(event: any) => void>>()

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
      serverSyncedSubs.clear()
      logger.info("✅ Connected")

      // Flush queued messages first
      while (messageQueue.length > 0) {
        const msg = messageQueue.shift()!
        _sendNow(msg)
      }

      // Prefer flushing pending requests if they exist
      if (pendingServerSubs.length > 0) {
        // Merge all pending batches & de-dup
        const merged = Array.from(
          new Set(pendingServerSubs.flat())
        )
        logger.info("⏩ Flushing pending server subs:", merged)
        // Now that we're going to send them to server, reflect them in "active"
        merged.forEach((ch) => activeServerSubs.value.add(ch))
        requestServerSubscribe(merged)
        pendingServerSubs.length = 0

      } else if (activeServerSubs.value.size > 0) {
        // No pendings → we re-request what we believe should be active (reconnect case)
        const channels = [...activeServerSubs.value]
        logger.info("🔄 Re-requesting server subs:", channels)
        requestServerSubscribe(channels)
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

      // fan-out to local listeners
      const listeners = localListeners.get(data.channel)
      if (listeners?.size) {
        for (const listener of listeners) listener(data)
      }
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

  // ---------------- server subscriptions ----------------
  function requestServerSubscribe(channels: string[]) {
    if (!channels?.length) return

    if (isConnected.value) {
      const toSend = channels.filter((ch) => !serverSyncedSubs.has(ch))
      if (toSend.length) {
        toSend.forEach((ch) => {
          activeServerSubs.value.add(ch)
          serverSyncedSubs.add(ch)
        })
        logger.info("📡 Request SUB →", toSend)
        send({ action: WSAction.SUBSCRIBE, channels: toSend })
      }
    } else {
      logger.info("⏳ Queue SUB (offline) →", channels)
      pendingServerSubs.push(Array.from(new Set(channels)))
    }
  }

  function requestServerUnsubscribe(channels: string[]) {
    if (!channels?.length) return
    channels.forEach((ch) => activeServerSubs.value.delete(ch))

    if (isConnected.value) {
      logger.info("📴 Request UNSUB →", channels)
      send({ action: WSAction.UNSUBSCRIBE, channels })
    } else {
      logger.info("(Offline) UNSUB ignored →", channels)
    }
  }

  // ---------------- local listeners ----------------
  function onChannel(channel: string, callback: (event: any) => void) {
    if (!localListeners.has(channel)) localListeners.set(channel, new Set())
    localListeners.get(channel)!.add(callback)
    logger.debug(
      `📡 local on("${channel}") (listeners=${localListeners.get(channel)!.size})`
    )
  }

  function offChannel(channel: string, callback: (event: any) => void) {
    const set = localListeners.get(channel)
    if (!set) return
    set.delete(callback)
    logger.debug(
      `🧹 Local off("${channel}") (listeners=${set.size})`
    )
    if (set.size === 0) localListeners.delete(channel)
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
    connect,
    send,
    requestServerSubscribe,
    requestServerUnsubscribe,
    onChannel,
    offChannel,
  }
})
