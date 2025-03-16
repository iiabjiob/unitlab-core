import { ref, onMounted, onUnmounted } from "vue";
import { config } from "@/config"; // Используем загруженный конфиг

export function useWebSockets(topic) {
  const data = ref(null);
  let socket = null;
  let manuallyClosed = false;

  const isDev = config.debug === "true";

  const connectWebSocket = () => {
    if (manuallyClosed) return;
    const wsProtocol = window.location.protocol === "https:" ? "wss" : "ws";
    const wsUrl = `${wsProtocol}://${window.location.host}/ws/${topic}`;
    if (isDev) console.log(`🔄 Connecting to WebSocket: ${wsUrl}`);
    socket = new WebSocket(wsUrl);

    socket.onopen = () => {
      if (isDev) console.log("✅ WebSocket connected.");
    };

    socket.onmessage = (event) => {
      data.value = JSON.parse(event.data);
    };

    socket.onerror = (error) => {
      if (isDev) console.error("⚠️ WebSocket error:", error);
    };

    socket.onclose = () => {
      if (isDev) console.warn("⚠️ WebSocket disconnected.");
      if (!manuallyClosed) {
        if (isDev) console.log("🔄 Attempting to reconnect in 5 seconds...");
        setTimeout(connectWebSocket, 5000);
      }
    };
  };

  onMounted(connectWebSocket);
  onUnmounted(() => {
    manuallyClosed = true; // Указываем, что WebSocket закрыт вручную
    if (socket) socket.close();
  });

  return { data };
}
