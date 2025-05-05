import { WSCommand } from '@/types/ws'
import { defineStore } from 'pinia'

interface WebSocketMessage {
  channel: string
  payload: any
}

interface WebSocketStoreState {
  socket: WebSocket | null
  isConnected: boolean
  isInitialized: boolean
  pendingSubscriptions: string[][]
  activeSubscriptions: Set<string>
  receivedData: Record<string, any>
  reconnectAttempts: number
  maxReconnectAttempts: number
  reconnectDelay: number
  maxReconnectDelay: number
  listeners: Map<string, Set<(payload: any) => void>>
}



export const useWebSocketStore = defineStore('websocketStore', {
  state: (): WebSocketStoreState => ({
    socket: null,
    isConnected: false,
    isInitialized: false,
    pendingSubscriptions: [],
    activeSubscriptions: new Set(),
    receivedData: {},
    reconnectAttempts: 0,
    maxReconnectAttempts: 5,
    reconnectDelay: 1000,
    maxReconnectDelay: 30000,
    listeners: new Map<string, Set<(payload: any) => void>>()
  }),

  actions: {
    connect(): void {
      const wsProtocol = window.location.protocol === 'https:' ? 'wss' : 'ws'
      const wsUrl = `${wsProtocol}://${window.location.host}/ws/ws`

      this.isInitialized = false

      console.log('🔌 Connecting to WebSocket...')
      this.socket = new WebSocket(wsUrl)

      this.socket.onopen = () => {
        console.log('✅ WebSocket connected!')
        this.isConnected = true
        this.isInitialized = true
        this.reconnectAttempts = 0

        if (this.activeSubscriptions.size > 0) {
          console.log('🔄 Subscribing to:', [...this.activeSubscriptions])
          this.subscribe([...this.activeSubscriptions])
        }

        this.pendingSubscriptions = []
      }

      this.socket.onclose = (event: CloseEvent) => {
        console.log('❌ WebSocket disconnected!', event)
        this.isConnected = false
        this.socket = null
        this.receivedData = {}
        this.reconnect()
      }

      this.socket.onmessage = (event: MessageEvent) => {
        const { channel, payload }: WebSocketMessage = JSON.parse(event.data)
        if (channel && payload !== undefined) {
          this.receivedData[channel] = payload
        }

        // ✅ Оповестить всех слушателей канала
        const channelListeners = this.listeners.get(channel)
        if (channelListeners) {
          for (const listener of channelListeners) {
            listener(payload)
          }
        }
      }

      this.socket.onerror = (error: Event) => {
        console.error('⚠️ WebSocket error:', error)
        this.socket?.close()
      }
    },

    // ✅ Унифицированная отправка
    send(message: WSCommand): void {
      if (this.socket && this.socket.readyState === WebSocket.OPEN) {
        this.socket.send(JSON.stringify(message))
        console.log('📤 Sent message to WS:', message)
      } else {
        console.warn('⚠️ WebSocket not connected, message not sent:', message)
      }
    },

    subscribe(channels: string[]): void {
      if (channels.length === 0) return
      channels.forEach((ch) => this.activeSubscriptions.add(ch))

      if (this.isConnected) {
        this.send({ action: 'subscribe', channels })
      } else {
        this.pendingSubscriptions.push(channels)
      }
    },

    unsubscribe(channels: string[]): void {
      channels.forEach((ch) => this.activeSubscriptions.delete(ch))

      if (this.isConnected) {
        this.send({ action: 'unsubscribe', channels })
      }
    },

    reconnect(): void {
      let delay: number

      if (this.reconnectAttempts < this.maxReconnectAttempts) {
        this.reconnectAttempts++
        delay = this.reconnectDelay * Math.pow(2, this.reconnectAttempts - 1)
      } else {
        delay = this.maxReconnectDelay
      }

      const seconds = (delay / 1000).toFixed(1)
      console.log(`⏳ Waiting ${seconds}s before reconnect attempt #${this.reconnectAttempts}...`)

      setTimeout(() => {
        console.log(`🔄 Reconnect attempt #${this.reconnectAttempts}...`)
        this.connect()
      }, delay)
    },

    subscribeToChannel(channel: string, callback: (payload: any) => void): void {
      if (!this.listeners.has(channel)) {
        this.listeners.set(channel, new Set())
      }
      this.listeners.get(channel)!.add(callback)
    },

    unsubscribeFromChannel(channel: string, callback: (payload: any) => void): void {
      this.listeners.get(channel)?.delete(callback)
    }

  }
})
