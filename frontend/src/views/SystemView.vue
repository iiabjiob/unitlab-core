<script setup>
import { ref, onMounted } from "vue";
import axios from "axios";
import { useWebSockets } from "@/composables/useWebSockets"; // Импорт WebSocket-компосабла
import PageHeader from "@/components/PageHeader.vue";
import LoadingSpinner from "@/components/LoadingSpinner.vue";
import AlertComponent from "@/components/AlertComponent.vue";

const apiSystemInfo = ref(null);
const message = ref("");

// Используем `useWebSockets` для получения данных о системе
const { data: systemInfo } = useWebSockets("system_info");

// Функция загрузки API-версии
const fetchSysInfo = async () => {
  try {
    const response = await axios.get("/api/system/info", { baseURL: "/" });
    apiSystemInfo.value = response.data;
  } catch {
    message.value = "Failed to load system information.";
  }
};

onMounted(() => {
  fetchSysInfo();
});


</script>

<template>
  <div>
    <PageHeader title="System Information" />

    <!-- Error message -->
    <AlertComponent v-if="message" :message="message" type="error" icon="⚠️" />

    <!-- Loading indicator -->
    <LoadingSpinner v-if="!systemInfo" size="small"/>

    <!-- System Info -->
    <div v-if="systemInfo">
      <p><strong>App Version:</strong> {{ apiSystemInfo?.version || "Loading..." }}</p>
      <p><strong>Hostname:</strong> {{ apiSystemInfo.hostname }}</p>
      <p><strong>IP Address:</strong> {{ apiSystemInfo.ip_address }}</p>
      <p><strong>Operating System:</strong> {{ apiSystemInfo.os }}</p>

      <p><strong>Uptime:</strong> {{ systemInfo.uptime }}</p>
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
      <p><strong>Signal Strength:</strong> {{ systemInfo.wifi.signal }}</p>
    </div>
  </div>
</template>
