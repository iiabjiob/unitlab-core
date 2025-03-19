<script setup>
import { ref, computed, onMounted, watchEffect } from "vue";
import { useMqtt } from "@/composables/useMqtt";
import { config, loadConfig } from "@/config"; // Импортируем loadConfig

const { data: healthStatus } = useMqtt("health_status");
const lastUpdate = ref(Date.now());
const checkInterval = config.health_check_interval; // Интервал проверки (изначально загружен)
const wasOffline = ref(true); // Флаг, который следит за оффлайном

const statusMessage = computed(() =>
  healthStatus.value?.status === "online" ? "🟢 Online" :
  healthStatus.value?.status === "offline" ? "🔴 Offline" :
  "🔄 Connecting..."
);

const statusClass = computed(() =>
  healthStatus.value?.status === "online" ? "text-green-500" :
  healthStatus.value?.status === "offline" ? "text-red-500" :
  "text-gray-500"
);

// Проверяем, когда в последний раз был статус
onMounted(() => {
  setInterval(() => {
    if (Date.now() - lastUpdate.value > checkInterval * 2) {
      console.log("⏳ No status update received, assuming FastAPI is down.");
      healthStatus.value = { status: "offline" };
      wasOffline.value = true; // Фиксируем, что приложение ушло в оффлайн
    }
  }, checkInterval);
});

// Если приходит новое сообщение — обновляем таймер
watchEffect(() => {
  if (healthStatus.value?.status === "online") {
    lastUpdate.value = Date.now();

    // Если приложение было оффлайн и стало онлайн — перезагружаем конфиг
    if (wasOffline.value) {
      console.log("🔄 Server is back online, reloading config...");
      loadConfig(); // Перезагружаем конфигурацию
      wasOffline.value = false;
    }
  }
});
</script>

<template>
  <div class="inline-flex gap-1 items-center text-sm" :class="statusClass">
    {{ statusMessage }}
  </div>
</template>
