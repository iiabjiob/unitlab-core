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

const syncSource = ref('');
const currentTime = ref(new Date());

const fetchTimeStatus = async () => {
  try {
    const response = await axios.get('/api/time/status');
    const data = response.data;

    if (data.ntp.synchronized) {
      syncSource.value = 'NTP';
      currentTime.value = new Date(data.ntp.current_time);
    } else if (data.ptp.synchronized) {
      syncSource.value = 'PTP';
      currentTime.value = new Date(data.ptp.current_time);
    } else {
      syncSource.value = '';
      currentTime.value = new Date(data.ntp.current_time || data.ptp.current_time);
    }
  } catch (error) {
    console.error('Ошибка получения данных о времени:', error);
  }
};

// Форматируем дату и время
const formattedDate = ref('');
const formattedTime = ref('');

const updateFormattedTime = () => {
  formattedDate.value = currentTime.value.toLocaleDateString('ru-RU', {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit'
  });

  formattedTime.value = currentTime.value.toLocaleTimeString('ru-RU', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  });
};

// Запуск таймеров
let syncInterval, clockInterval;
onMounted(() => {
  fetchTimeStatus();
  syncInterval = setInterval(fetchTimeStatus, 10000);

  clockInterval = setInterval(() => {
    currentTime.value = new Date(currentTime.value.getTime() + 1000);
    updateFormattedTime();
  }, 1000);
});

// Очищаем интервалы
onUnmounted(() => {
  clearInterval(syncInterval);
  clearInterval(clockInterval);
});
</script>
