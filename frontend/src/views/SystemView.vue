<script setup>
import { computed, onMounted, onUnmounted } from "vue";
import PageHeader from "@/components/PageHeader.vue";
import LoadingSpinner from "@/components/LoadingSpinner.vue";
import { useWebSocketStore } from "@/stores/websocket";
import InfoRowComponent from "@/components/InfoRowComponent.vue";

const wsStore = useWebSocketStore();

onMounted(() => {
  wsStore.subscribe(["system_info"]);
});

onUnmounted(() => {
  wsStore.unsubscribe(["system_info"]); // ⬅ Отписываемся при уходе со страницы
});

// Полные данные
const systemInfo = computed(() => wsStore.receivedData["system_info"] ?? {});

// Загрузка считается завершённой, когда пришли все ключевые поля
const isLoaded = computed(() => {
  const info = systemInfo.value;
  return (
    info.cpu &&
    info.ram &&
    info.disk &&
    info.os &&
    info.uptime &&
    info.temperature &&
    info.wifi
  );
});

</script>

<template>
  <div>
    <PageHeader title="System Information" />

    <!-- Показываем спинер, пока не загружены все ключевые данные -->
    <LoadingSpinner size="small" v-if="!isLoaded" />

    <!-- Когда данные готовы, показываем всю страницу -->
    <div v-else class="space-y-3 text-sm">
      <!-- CPU -->
      <div>
        <h2 class="font-bold mb-2">CPU</h2>
        <InfoRowComponent label="CPU Usage">{{ systemInfo.cpu }}</InfoRowComponent>
        <InfoRowComponent label="CPU Temperature">{{ systemInfo.temperature }}</InfoRowComponent>
      </div>

      <!-- RAM -->
      <div>
        <h2 class="font-bold mb-2">Memory (RAM)</h2>
        <InfoRowComponent label="Total RAM">{{ systemInfo.ram.total }} GB</InfoRowComponent>
        <InfoRowComponent label="Used RAM">{{ systemInfo.ram.used }} GB</InfoRowComponent>
        <InfoRowComponent label="Available RAM">{{ systemInfo.ram.available }} GB</InfoRowComponent>
      </div>

      <!-- Disk -->
      <div>
        <h2 class="font-bold mb-2">Disk</h2>
        <InfoRowComponent label="Total Disk">{{ systemInfo.disk.total }} GB</InfoRowComponent>
        <InfoRowComponent label="Used Disk">{{ systemInfo.disk.used }} GB</InfoRowComponent>
        <InfoRowComponent label="Free Disk">{{ systemInfo.disk.free }} GB</InfoRowComponent>
      </div>

      <!-- System Info -->
      <div>
        <h2 class="font-bold mb-2">System</h2>
        <InfoRowComponent label="Operating System">{{ systemInfo.os }}</InfoRowComponent>
        <InfoRowComponent label="System Uptime">{{ systemInfo.uptime }}</InfoRowComponent>
      </div>

      <!-- Wi-Fi -->
      <div>
        <h2 class="font-bold mb-2">Wi-Fi</h2>
        <InfoRowComponent label="Wi-Fi SSID">{{ systemInfo.wifi.ssid }}</InfoRowComponent>
        <InfoRowComponent label="Wi-Fi Signal">{{ systemInfo.wifi.signal }}</InfoRowComponent>
      </div>
    </div>
  </div>
</template>
