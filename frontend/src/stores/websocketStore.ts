// src/stores/websocketStore.ts
import { defineStore } from 'pinia'
import { handleWsEvent } from '@/services/wsHandler'
import type { WSEvent } from '@/types/ws/events'
import { WSAction } from "@/types/ws/messages"
import { getLogger } from '@/utils/logger'

const logger = getLogger("WS")

interface WebSocketStoreState {
  socket: WebSocket | null
  isConnected: boolean
  pendingServerSubs: string[][]             // batched pending subs to request on connect
  activeServerSubs: Set<string>             // channels requested from server
  receivedData: Record<string, any>
  reconnectAttempts: number
  maxReconnectAttempts: number
  reconnectDelay: number
  maxReconnectDelay: number
  localListeners: Map<string, Set<(event: any) => void>> // local callbacks by channel
}

export const useWebSocketStore = defineStore('websocketStore', {
  state: (): WebSocketStoreState => ({
    socket: null,
    isConnected: false,
    pendingServerSubs: [],
    activeServerSubs: new Set(),
    receivedData: {},
    reconnectAttempts: 0,
    maxReconnectAttempts: 5,
    reconnectDelay: 1000,
    maxReconnectDelay: 30000,
    localListeners: new Map()
  }),

  actions: {
    connect(): void {
      const wsProtocol = window.location.protocol === 'https:' ? 'wss' : 'ws'
      const wsUrl = `${wsProtocol}://${window.location.host}/ws/ws`

      logger.info('🔌 Connecting')

      this.socket = new WebSocket(wsUrl)

      this.socket.onopen = () => {
        this.isConnected = true
        this.reconnectAttempts = 0
        logger.info('✅ Connected')

        // re-request server subscriptions
        if (this.activeServerSubs.size > 0) {
          const channels = [...this.activeServerSubs]
          logger.info('🔄 Re-requesting server subs:', channels)
          this.requestServerSubscribe(channels)
        }

        // flush delayed sub-requests
        if (this.pendingServerSubs.length > 0) {
          for (const batch of this.pendingServerSubs) {
            logger.info('⏩ Flushing pending server subs:', batch)
            this.requestServerSubscribe(batch)
          }
          this.pendingServerSubs = []
        }
      }

      this.socket.onclose = (event: CloseEvent) => {
        logger.warn('💥 Disconnected', event)
        this.isConnected = false
        this.socket = null
        this.receivedData = {}
        this._scheduleReconnect()
      }

      this.socket.onmessage = (event: MessageEvent) => {
        const data: WSEvent = JSON.parse(event.data)
        // dispatch to domain stores
        handleWsEvent(data)

        // fan-out to local listeners (components)
        const listeners = this.localListeners.get(data.channel)
        if (listeners?.size) {
          // Note: we pass the whole event so listeners can discriminate by channel
          for (const listener of listeners) listener(data)
        }
      }

      this.socket.onerror = (error: Event) => {
        logger.error('⚠️ Error', error)
        this.socket?.close()
      }
    },

    // -------- low-level send ----------
    send(message: Record<string, any>): void {
      if (this.socket && this.socket.readyState === WebSocket.OPEN) {
        logger.debug('📤 Send', message)
        this.socket.send(JSON.stringify(message))
      } else {
        logger.warn('💤 Not connected, dropped', message)
      }
    },

    // -------- SERVER subscription API (request server to filter / push) ----------
    requestServerSubscribe(channels: string[]): void {
      if (!channels?.length) return
      channels.forEach((ch) => this.activeServerSubs.add(ch))

      if (this.isConnected) {
        logger.info(`📡 Request SUB →`, channels)
        this.send({ action: WSAction.SUBSCRIBE, channels })
      } else {
        logger.info(`⏳ Queue SUB (offline) →`, channels)
        this.pendingServerSubs.push(channels)
      }
    },

    requestServerUnsubscribe(channels: string[]): void {
      if (!channels?.length) return
      channels.forEach((ch) => this.activeServerSubs.delete(ch))

      if (this.isConnected) {
        logger.info(`📴 Request UNSUB →`, channels)
        this.send({ action: WSAction.UNSUBSCRIBE, channels })
      } else {
        logger.info(`(Offline) UNSUB ignored →`, channels)
      }
    },

    // -------- LOCAL subscription API (components register callbacks) ----------
    onChannel(channel: string, callback: (event: any) => void): void {
      if (!this.localListeners.has(channel)) this.localListeners.set(channel, new Set())
      this.localListeners.get(channel)!.add(callback)
      logger.debug(`📡 local on("${channel}") (listeners=${this.localListeners.get(channel)!.size})`)
    },

    offChannel(channel: string, callback: (event: any) => void): void {
      const set = this.localListeners.get(channel)
      if (!set) return
      set.delete(callback)
      logger.debug(`🧹 Local off("${channel}") (listeners=${set.size})`)
      if (set.size === 0) this.localListeners.delete(channel)
    },

    // -------- reconnect backoff ----------
    _scheduleReconnect(): void {
      let delay: number
      if (this.reconnectAttempts < this.maxReconnectAttempts) {
        this.reconnectAttempts++
        delay = this.reconnectDelay * Math.pow(2, this.reconnectAttempts - 1)
      } else {
        delay = this.maxReconnectDelay
      }
      const seconds = (delay / 1000).toFixed(1)
      logger.info(`⏳ Reconnect in ${seconds}s (attempt #${this.reconnectAttempts})`)
      setTimeout(() => {
        logger.info(`🔄 Reconnect attempt #${this.reconnectAttempts}`)
        this.connect()
      }, delay)
    },
  }
})
