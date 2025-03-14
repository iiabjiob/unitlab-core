import { ref } from "vue";
import axios from "axios";

const serverAvailable = ref(true); // Глобальное состояние сервера
const checkInterval = Number(import.meta.env.VITE_SERVER_CHECK_INTERVAL) || 10000;
let interval;

const checkServerStatus = async () => {
  try {
    await axios.get("/api/health", { timeout: 3000 }); // 3 сек. таймаут
    if (!serverAvailable.value) {
      console.info("✅ Server is back online.");
    }
    serverAvailable.value = true;
  } catch {
    if (serverAvailable.value) {
      console.warn("⚠️ Server is unavailable. Retrying...");
    }
    serverAvailable.value = false;
  }
};

const startChecking = () => {
  checkServerStatus();
  interval = setInterval(checkServerStatus, checkInterval);
};

const stopChecking = () => {
  clearInterval(interval);
};

export function useServerStatus() {
  return { serverAvailable, startChecking, stopChecking };
}
