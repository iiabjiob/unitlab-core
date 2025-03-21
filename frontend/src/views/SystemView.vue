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

// Extract system info
const systemInfo = computed(() => wsStore.receivedData["system_info"] ?? {});

</script>

<template>
  <div>
    <PageHeader title="System Information" />

    <LoadingSpinner size="small" v-if="!systemInfo.cpu"/>

    <InfoRowComponent label="CPU Usage">{{ systemInfo.cpu ?? "N/A" }}</InfoRowComponent>
    <InfoRowComponent label="Total RAM">{{ systemInfo.ram?.total ?? "N/A" }} GB</InfoRowComponent>
    <InfoRowComponent label="Used RAM">{{ systemInfo.ram?.used ?? "N/A" }} GB</InfoRowComponent>
    <InfoRowComponent label="Available RAM">{{ systemInfo.ram?.available ?? "N/A" }} GB</InfoRowComponent>
    <InfoRowComponent label="Total Disk">{{ systemInfo.disk?.total ?? "N/A" }} GB</InfoRowComponent>
    <InfoRowComponent label="Used Disk">{{ systemInfo.disk?.used ?? "N/A" }} GB</InfoRowComponent>
    <InfoRowComponent label="Free Disk">{{ systemInfo.disk?.free ?? "N/A" }} GB</InfoRowComponent>
    <InfoRowComponent label="Operating System">{{ systemInfo.os ?? "Unknown OS" }}</InfoRowComponent>
    <InfoRowComponent label="System Uptime">{{ systemInfo.uptime ?? "N/A" }}</InfoRowComponent>
    <InfoRowComponent label="CPU Temperature">{{ systemInfo.temperature ?? "N/A" }}</InfoRowComponent>
    <InfoRowComponent label="Wi-Fi SSID">{{ systemInfo.wifi?.ssid ?? "N/A" }}</InfoRowComponent>
    <InfoRowComponent label="Wi-Fi Signal">{{ systemInfo.wifi?.signal ?? "N/A" }}</InfoRowComponent>
  </div>
</template>
