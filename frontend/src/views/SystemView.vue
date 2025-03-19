<script setup>
import { ref, onMounted } from "vue";
import axios from "axios";
import { useMqtt } from "@/composables/useMqtt";
import PageHeader from "@/components/PageHeader.vue";
import LoadingSpinner from "@/components/LoadingSpinner.vue";
import AlertComponent from "@/components/AlertComponent.vue";
import SystemInfoItemComponent from "@/components/SystemInfoItemComponent.vue";

const { data: systemInfo } = useMqtt("system_info");
const apiSystemInfo = ref(null);
const message = ref("");

// Fetch static system info via API
const fetchSysInfo = async () => {
  try {
    const response = await axios.get("/api/system/info", { baseURL: "/" });
    apiSystemInfo.value = response.data;
  } catch {
    message.value = "Failed to load system information.";
  }
};

onMounted(fetchSysInfo);
</script>

<template>
  <div>
    <PageHeader title="System Information" />
    <AlertComponent v-if="message" :message="message" type="error" icon="⚠️" />

    <div v-if="apiSystemInfo">
      <SystemInfoItemComponent label="App Version">
        {{ apiSystemInfo.version || "Loading..." }}
      </SystemInfoItemComponent>
      <SystemInfoItemComponent label="Hostname">
        {{ apiSystemInfo.hostname }}
      </SystemInfoItemComponent>
      <SystemInfoItemComponent label="IP Address">
        {{ apiSystemInfo.ip_address }}
      </SystemInfoItemComponent>
      <SystemInfoItemComponent label="Operating System">
        {{ apiSystemInfo.os }}
      </SystemInfoItemComponent>
    </div>

    <div v-if="systemInfo">
      <SystemInfoItemComponent label="Uptime">
        {{ systemInfo.uptime }}
      </SystemInfoItemComponent>
      <SystemInfoItemComponent label="CPU Load">
        {{ systemInfo.cpu_usage }}
      </SystemInfoItemComponent>
      <SystemInfoItemComponent label="Temperature">
        {{ systemInfo.temperature }}
      </SystemInfoItemComponent>

      <h2 class="mt-4 font-semibold">Memory</h2>
      <SystemInfoItemComponent label="Total">
        {{ systemInfo.ram.total }} GB
      </SystemInfoItemComponent>
      <SystemInfoItemComponent label="Used">
        {{ systemInfo.ram.used }} GB
      </SystemInfoItemComponent>
      <SystemInfoItemComponent label="Available">
        {{ systemInfo.ram.available }} GB
      </SystemInfoItemComponent>

      <h2 class="mt-4 font-semibold">Disk</h2>
      <SystemInfoItemComponent label="Total">
        {{ systemInfo.disk.total }} GB
      </SystemInfoItemComponent>
      <SystemInfoItemComponent label="Used">
        {{ systemInfo.disk.used }} GB
      </SystemInfoItemComponent>
      <SystemInfoItemComponent label="Free">
        {{ systemInfo.disk.free }} GB
      </SystemInfoItemComponent>

      <h2 class="mt-4 font-semibold">Wi-Fi</h2>
      <SystemInfoItemComponent label="SSID">
        {{ systemInfo.wifi.ssid }}
      </SystemInfoItemComponent>
      <SystemInfoItemComponent label="Signal Strength">
        {{ systemInfo.wifi.signal }}
      </SystemInfoItemComponent>
    </div>

    <LoadingSpinner v-if="!apiSystemInfo || !systemInfo" size="small"/>
  </div>
</template>
