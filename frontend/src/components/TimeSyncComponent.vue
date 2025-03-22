<script setup>

import { computed, onMounted, ref, watch } from "vue";
import { useWebSocketStore } from "@/stores/websocket";

const wsStore = useWebSocketStore();

// Состояние
const localTime = ref(new Date());
const syncBaseTime = ref(null);        // Время из WebSocket
const syncStartTime = ref(null);       // Когда оно пришло

// Подписка на канал
onMounted(() => {
  wsStore.subscribe(["time_status"]);

  // Обновляем локальные часы каждую секунду
  setInterval(() => {
    localTime.value = new Date();
  }, 1000);
});

// Обновляем базу синхронизированного времени при новом сообщении
watch(
  () => wsStore.receivedData["time_status"],
  (newData) => {
    if (newData?.timestamp) {
      syncBaseTime.value = new Date(newData.timestamp);
      syncStartTime.value = new Date(); // текущее время клиента
    }
  },
  { immediate: true }
);

// Источник времени
const timeSource = computed(() => wsStore.receivedData["time_status"]?.source ?? "LOCAL");

const tick = ref(0);
setInterval(() => tick.value++, 1000);

const formattedTime = computed(() => {
  tick.value; // делаем computed реактивным
  if (syncBaseTime.value && syncStartTime.value) {
    const now = new Date();
    const elapsed = now.getTime() - syncStartTime.value.getTime();
    return new Date(syncBaseTime.value.getTime() + elapsed).toLocaleString();
  }
  return localTime.value.toLocaleString();
});

const sourceLabels = {
  PTP: "PTP",
  NTP: "NTP",
  LOCAL: "Local Time",
};

const timeSourceLabel = computed(() => sourceLabels[timeSource.value] ?? "Неизвестно");

</script>

<template>
  <div class="flex flex-wrap text-xs text-gray-500 gap-x-2">
    <span>{{ timeSourceLabel }}</span>
    <span class="tabular-nums font-mono text-right">{{ formattedTime }}</span>
  </div>
</template>
