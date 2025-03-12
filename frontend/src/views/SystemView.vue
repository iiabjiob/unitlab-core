<script setup>
import { ref, onMounted } from "vue";
import axios from "axios";
import PageHeader from '@/components/PageHeader.vue';
import LoadingSpinner from "@/components/LoadingSpinner.vue";

const systemInfo = ref(null);
const message = ref("");
const isLoading = ref(true);

const fetchSystemInfo = async () => {
  try {
    const response = await axios.get("/api/system/info", { baseURL: "/" });
    systemInfo.value = response.data;
  } catch {
    message.value = "Ошибка загрузки информации о системе";
  } finally {
      isLoading.value = false;
    }
};

onMounted(fetchSystemInfo);
</script>

<template>
  <div>
    <PageHeader title="System info" />

    <p v-if="message" class="text-red-500">{{ message }}</p>

    <!-- Индикатор загрузки -->
    <LoadingSpinner v-if="isLoading" size="small"/>

    <!-- Информация -->
    <div v-if="!isLoading && systemInfo">
      <p><strong>Hostname:</strong> {{ systemInfo.hostname }}</p>
      <p><strong>IP Address:</strong> {{ systemInfo.ip_address }}</p>
      <p><strong>OS:</strong> {{ systemInfo.os }}</p>
      <p><strong>Uptime:</strong> {{ systemInfo.uptime }} sec</p>
      <p><strong>CPU Load:</strong> {{ systemInfo.cpu_usage }}</p>
      <p><strong>Temperature:</strong> {{ systemInfo.temperature }}</p>

      <h2 class="mt-4 font-semibold">Memory</h2>
      <p><strong>Total:</strong> {{ systemInfo.ram.total }} GB</p>
      <p><strong>Used:</strong> {{ systemInfo.ram.used }} GB</p>
      <p><strong>Available:</strong> {{ systemInfo.ram.available }} GB</p>

      <h2 class="mt-4 font-semibold">Disk</h2>
      <p><strong>Total:</strong> {{ systemInfo.disk.total }} GB</p>
      <p><strong>Used:</strong> {{ systemInfo.disk.used }} GB</p>
      <p><strong>Free:</strong> {{ systemInfo.disk.free }} GB</p>

      <h2 class="mt-4 font-semibold">Wi-Fi</h2>
      <p><strong>SSID:</strong> {{ systemInfo.wifi.ssid }}</p>
      <p><strong>Signal:</strong> {{ systemInfo.wifi.signal }}</p>
    </div>

  </div>
</template>
