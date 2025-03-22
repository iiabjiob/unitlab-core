import { defineStore } from "pinia";

// WebSocket store to manage real-time communication with the server
export const useWebSocketStore = defineStore("websocket", {
  state: () => ({
    socket: null,                        // Active WebSocket instance
    isConnected: false,                  // Connection state
    pendingSubscriptions: [],            // Subscriptions requested before connection is open
    activeSubscriptions: new Set(),      // All currently active subscriptions
    receivedData: {},                    // Universal storage for all incoming data

    reconnectAttempts: 0,                // Number of current reconnect attempts
    maxReconnectAttempts: 5,             // Max attempts before switching to fixed delay
    reconnectDelay: 1000,                // Base delay for exponential backoff (in ms)
    maxReconntetDelay: 30000             // Constant delay after maxReconnectAttempts (in ms)
  }),

  actions: {
    /**
     * Initiates a WebSocket connection and sets up handlers.
     */
    connect() {

      const wsProtocol = window.location.protocol === "https:" ? "wss" : "ws";
      const wsUrl = `${wsProtocol}://${window.location.host}/ws/ws`;

      console.log("🔌 Connecting to WebSocket...");

      this.socket = new WebSocket(wsUrl);


      this.socket.onopen = () => {
        console.log("✅ WebSocket connected!");
        this.isConnected = true;
        this.reconnectAttempts = 0; // Reset on successful connection

        // 🔄 Resubscribe to all active subscriptions after reconnect
        if (this.activeSubscriptions.size > 0) {
          console.log("🔄 Resubscribing to:", [...this.activeSubscriptions]);
          this.subscribe([...this.activeSubscriptions]);
        }

        // Clear any pending subscriptions (they're now handled)
        this.pendingSubscriptions = [];
      };

      this.socket.onclose = (event) => {
        console.log("❌ WebSocket disconnected!", event);
        this.isConnected = false;
        this.socket = null;
        this.reconnect(); // Try to reconnect
      };

      this.socket.onmessage = (event) => {
        const { channel, payload } = JSON.parse(event.data);
        if (channel && payload) {
          this.receivedData[channel] = payload;
        }
      };

      this.socket.onerror = (error) => {
        console.error("⚠️ WebSocket error:", error);
        this.socket.close(); // Ensure clean reconnection path
      };
    },

    /**
     * Sends a subscription request or queues it if the socket isn't open.
     * @param {string[]} dataTypes - Array of data types to subscribe to.
     */
    subscribe(dataTypes) {
      // Store all subscriptions in the active set (no duplicates)
      dataTypes.forEach((type) => this.activeSubscriptions.add(type));

      if (this.socket && this.socket.readyState === WebSocket.OPEN) {
        console.log("📤 Sending subscription request to the server:", dataTypes);
        this.socket.send(JSON.stringify({ action: "subscribe", channels: dataTypes }));
      } else {
        console.log("🕐 WebSocket not connected yet, saving subscription request:", dataTypes);
        this.pendingSubscriptions.push(dataTypes);
      }
    },

    /**
     * Unsubscribes from the specified WebSocket data types.
     *
     * - Removes the subscription from `activeSubscriptions`.
     * - Sends an `unsubscribe` request to the server.
     * - Prevents receiving unnecessary WebSocket updates.
     *
     * @param {string[]} dataTypes - Array of data types to unsubscribe from.
     */
    unsubscribe(dataTypes) {
      dataTypes.forEach((type) => this.activeSubscriptions.delete(type));

      if (this.socket && this.socket.readyState === WebSocket.OPEN) {
        console.log("📤 Unsubscribing from:", dataTypes);
        this.socket.send(JSON.stringify({ action: "unsubscribe", channels: dataTypes }));
      }
    },

    /**
     * Handles reconnection logic using exponential backoff,
     * switching to fixed delay after reaching the max attempt limit.
     */
    reconnect() {
      let delay;

      if (this.reconnectAttempts < this.maxReconnectAttempts) {
        this.reconnectAttempts++;
        delay = this.reconnectDelay * Math.pow(2, this.reconnectAttempts - 1);
      } else {
        delay = this.maxReconntetDelay;
      }

      const seconds = (delay / 1000).toFixed(1);
      console.log(`⏳ Waiting ${seconds}s before reconnect attempt #${this.reconnectAttempts}...`);

      setTimeout(() => {
        console.log(`🔄 Reconnect attempt #${this.reconnectAttempts}...`);
        this.connect();
      }, delay);
    },
  },
});
