import { ref, onMounted, onUnmounted } from "vue";
import axios from "axios";

export function useHealthCheck() {
  const serverAvailable = ref(true);
  let socket = null;
  let healthInterval = null;
  let manuallyClosed = false;

  const isDev = Boolean(import.meta.env.VITE_APP_DEBUG );

  const checkServerViaAPI = async () => {
    try {
      const response = await axios.get("/api/health");
      if (response.data.status === "ok") {
       if (isDev) console.log("✅ Server is online. Stopping polling.");
        serverAvailable.value = true;
        clearInterval(healthInterval); // ✅ Останавливаем Polling, если сервер снова работает
        healthInterval = null; // ✅ Сбрасываем переменную, чтобы избежать дубликатов
        connectWebSocket(); // 🔄 Переподключаем WebSocket
      }
    } catch {
      serverAvailable.value = false;
    }
  };

  const connectWebSocket = () => {
    if (manuallyClosed) return;

    const wsProtocol = window.location.protocol === "https:" ? "wss" : "ws";
    const wsUrl = `${wsProtocol}://${window.location.host}/ws/health`;
    if (isDev) console.log(`🔄 Connecting to WebSocket: ${wsUrl}`);

    if (socket) {
      if (isDev) console.warn("❌ Closing existing WebSocket before reconnecting...");
      socket.close();
      socket = null;
    }

    socket = new WebSocket(wsUrl);

    socket.onopen = () => {
      if (isDev) console.log("✅ WebSocket connected.");
      serverAvailable.value = true;

      // ✅ Останавливаем Polling, если WebSocket подключился
      if (healthInterval) {
        clearInterval(healthInterval);
        healthInterval = null; // ✅ Сбрасываем, чтобы избежать повторного `setInterval`
      }
    };

    socket.onmessage = (event) => {
      const msg = JSON.parse(event.data);
      serverAvailable.value = msg.status === "ok";
    };

    socket.onerror = (error) => {
      if (isDev) console.error("⚠️ WebSocket error:", error);
    };

    socket.onclose = () => {
      if (isDev) console.warn("⚠️ WebSocket disconnected. Switching to polling...");
      serverAvailable.value = false;

      if (!healthInterval) {
        healthInterval = setInterval(checkServerViaAPI, 10000); // ✅ Включаем Polling (раз в 10 сек)
      }
    };
  };

  onMounted(() => {
    connectWebSocket();
  });

  onUnmounted(() => {
    manuallyClosed = true;
    if (socket) socket.close();
    clearInterval(healthInterval);
  });

  return { serverAvailable };
}
