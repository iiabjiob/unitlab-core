import { defineStore } from 'pinia'

interface WebSocketMessage {
  channel: string
  payload: any
}

interface WebSocketStoreState {
  socket: WebSocket | null
  isConnected: boolean
  pendingSubscriptions: string[][]
  activeSubscriptions: Set<string>
  receivedData: Record<string, any>
  reconnectAttempts: number
  maxReconnectAttempts: number
  reconnectDelay: number
  maxReconnectDelay: number
}

export const useWebSocketStore = defineStore('websocket', {
  state: (): WebSocketStoreState => ({
    socket: null,
    isConnected: false,
    pendingSubscriptions: [],
    activeSubscriptions: new Set(),
    receivedData: {},
    reconnectAttempts: 0,
    maxReconnectAttempts: 5,
    reconnectDelay: 1000,
    maxReconnectDelay: 30000
  }),

  actions: {
    connect(): void {
      const wsProtocol = window.location.protocol === 'https:' ? 'wss' : 'ws'
      const wsUrl = `${wsProtocol}://${window.location.host}/ws/ws`

      console.log('🔌 Connecting to WebSocket...')
      this.socket = new WebSocket(wsUrl)

      this.socket.onopen = () => {
        console.log('✅ WebSocket connected!')
        this.isConnected = true
        this.reconnectAttempts = 0

        if (this.activeSubscriptions.size > 0) {
          console.log('🔄 Resubscribing to:', [...this.activeSubscriptions])
          this.subscribe([...this.activeSubscriptions])
        }

        this.pendingSubscriptions = []
      }

      this.socket.onclose = (event: CloseEvent) => {
        console.log('❌ WebSocket disconnected!', event)
        this.isConnected = false
        this.socket = null
        this.reconnect()
      }

      this.socket.onmessage = (event: MessageEvent) => {
        const { channel, payload }: WebSocketMessage = JSON.parse(event.data)
        if (channel && payload !== undefined) {
          this.receivedData[channel] = payload
        }
      }

      this.socket.onerror = (error: Event) => {
        console.error('⚠️ WebSocket error:', error)
        this.socket?.close()
      }
    },

    subscribe(dataTypes: string[]): void {
      dataTypes.forEach((type) => this.activeSubscriptions.add(type))

      if (this.socket && this.socket.readyState === WebSocket.OPEN) {
        console.log('📤 Sending subscription request to the server:', dataTypes)
        this.socket.send(JSON.stringify({ action: 'subscribe', channels: dataTypes }))
      } else {
        console.log('🕐 WebSocket not connected yet, saving subscription request:', dataTypes)
        this.pendingSubscriptions.push(dataTypes)
      }
    },

    unsubscribe(dataTypes: string[]): void {
      dataTypes.forEach((type) => this.activeSubscriptions.delete(type))

      if (this.socket && this.socket.readyState === WebSocket.OPEN) {
        console.log('📤 Unsubscribing from:', dataTypes)
        this.socket.send(JSON.stringify({ action: 'unsubscribe', channels: dataTypes }))
      }
    },

    send(message: any): void {
      if (this.socket && this.socket.readyState === WebSocket.OPEN) {
        this.socket.send(JSON.stringify(message))
        console.log('📤 Sent message to WS:', message)
      } else {
        console.warn('⚠️ WebSocket not connected, message not sent:', message)
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
    }
  }
})
