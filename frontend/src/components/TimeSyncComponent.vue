<template>
  <div class="inline-flex gap-1 items-center text-sm text-gray-500">
    <span v-if="timeSync?.synchronized">{{ timeSync.source }}</span>
    <span v-else>*</span>
    <span>{{ formattedDate }}</span>
    <span class="tabular-nums">{{ formattedTime }}</span>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, watchEffect } from "vue";
import { useWebSockets } from "@/composables/useWebSockets"; // Используем WebSocket-компосабл

// Используем WebSocket для получения данных о синхронизации времени
const { data: timeSync } = useWebSockets("time_sync");

// Локальное время (обновляется между обновлениями с сервера)
const currentTime = ref(new Date());

// Функция обновления отображаемого времени
const formattedDate = computed(() =>
  currentTime.value.toLocaleDateString("en-GB", {
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
  })
);

const formattedTime = computed(() =>
  currentTime.value.toLocaleTimeString("en-GB", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  })
);

// Интервал для плавного обновления локального времени
let clockInterval;

onMounted(() => {
  // Если приходит новое время с WebSocket, обновляем локальное время
  watchEffect(() => {
    if (timeSync?.value?.synchronized && timeSync.value.current_time) {
      currentTime.value = new Date(timeSync.value.current_time);
    }
  });

  // Каждую секунду увеличиваем локальное время на 1 сек.
  clockInterval = setInterval(() => {
    currentTime.value = new Date(currentTime.value.getTime() + 1000);
  }, 1000);
});

onUnmounted(() => {
  clearInterval(clockInterval);
});
</script>
