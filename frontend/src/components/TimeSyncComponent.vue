<template>
  <div class="inline-flex gap-1 items-center text-sm text-gray-500">
    <span v-if="syncSource">{{ syncSource }}</span>
    <span v-else>*</span>
    <span>{{ formattedDate }}</span>
    <span class="tabular-nums">{{ formattedTime }}</span>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue';
import axios from 'axios';
import { useServerStatus } from '@/composables/useServerStatus';

const { serverAvailable } = useServerStatus(); // 🔄 Глобальный статус сервера

const syncSource = ref('');
const currentTime = ref(new Date());
const syncIntervalTime = import.meta.env.VITE_TIME_SYNC_INTERVAL || 60000; // From frontend/.env or 60s

const fetchTimeStatus = async () => {
  try {
    if (!serverAvailable.value) return; // ⛔ Отключаем API-запросы, если сервер недоступен
    const response = await axios.get('/api/time/status');
    const data = response.data;

    if (data.synchronized) {
      syncSource.value = data.source;
      currentTime.value = new Date(data.current_time);
    } else {
      syncSource.value = '*';
      currentTime.value = new Date(data.current_time || new Date()); // Default to local time if missing
    }
  } catch (error) {
    console.error('Error retrieving time data:', error);
  }
};

// Formatting functions
const formattedDate = ref('');
const formattedTime = ref('');

const updateFormattedTime = () => {
  formattedDate.value = currentTime.value.toLocaleDateString('en-GB', {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit'
  });

  formattedTime.value = currentTime.value.toLocaleTimeString('en-GB', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  });
};

// Timers
let syncInterval, clockInterval;
onMounted(() => {
  fetchTimeStatus();
  syncInterval = setInterval(fetchTimeStatus, syncIntervalTime);

  clockInterval = setInterval(() => {
    currentTime.value = new Date(currentTime.value.getTime() + 1000);
    updateFormattedTime();
  }, 1000);
});

// Cleanup
onUnmounted(() => {
  clearInterval(syncInterval);
  clearInterval(clockInterval);
});
</script>
