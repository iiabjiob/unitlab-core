import { ref, onMounted, onUnmounted } from "vue";

export function useWebSockets(topic) {
  const data = ref(null);
  let socket = null;
  let manuallyClosed = false; // Флаг для отслеживания ручного закрытия

  const isDev = import.meta.env.VITE_APP_DEBUG === true;

  const connectWebSocket = () => {
    if (manuallyClosed) return;
    const wsProtocol = window.location.protocol === "https:" ? "wss" : "ws";
    const wsUrl = `${wsProtocol}://${window.location.host}/ws/${topic}`;
    if (isDev) console.log(`Connecting to WebSocket at ${wsUrl}`);
    socket = new WebSocket(wsUrl);

    socket.onopen = () => {
      if (isDev) console.log("WebSocket connection established");
    };

    socket.onmessage = (event) => {
      data.value = JSON.parse(event.data);
    };

    socket.onerror = (error) => {
      if (isDev) console.error("WebSocket error:", error);
    };

    socket.onclose = () => {
      console.warn("⚠️ WebSocket disconnected.");
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
