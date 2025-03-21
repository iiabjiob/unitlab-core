<script setup>

  import { computed, onMounted, ref } from "vue";
  import { useWebSocketStore } from "@/stores/websocket";

  const wsStore = useWebSocketStore();

  const localTime = ref(new Date().toLocaleString());

  onMounted(() => {
    wsStore.subscribe(["time_sync"]);

    // Обновляем локальное время каждую секунду
    setInterval(() => {
      localTime.value = new Date().toLocaleString();
    }, 1000);
  });

  // Вытаскиваем последние данные по time_sync
  const timeData = computed(() => wsStore.receivedData["time_sync"] ?? {});

  // Источник времени (PTP, NTP, или * / unknown)
  const timeSource = computed(() => timeData.value.time_source ?? "*");

  // Форматированное время
  const formattedTime = computed(() => {
    if (timeSource.value === "*" || !timeData.value.time) {
      return localTime.value;
    }
    return timeData.value.time;
  });

</script>

<template>
  <div class="flex flex-wrap text-xs text-gray-500 gap-x-1">
    <span>{{ timeSource }}</span>
    <span class="tabular-nums font-mono text-right">{{ formattedTime }}</span>
  </div>
</template>
