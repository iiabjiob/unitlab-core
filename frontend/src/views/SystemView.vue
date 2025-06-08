<template>
  <div>
    <!-- <PageHeader title="System Information" /> -->

    <!-- Показываем спиннер, пока нет данных -->
    <LoadingSpinner size="small" position="left" v-if="!isLoaded" />

    <!-- Когда данные готовы, показываем -->
    <div v-else class="space-y-3 text-sm">

      <!-- APP Version -->
      <div>
        <InfoRowComponent label="Version">{{ systemInfo?.app_version }}</InfoRowComponent>
      </div>

      <!-- Network -->
      <div>
        <h2 class="font-bold mb-2">Network</h2>
        <InfoRowComponent label="Host">{{ systemInfo?.host_name }}</InfoRowComponent>
        <InfoRowComponent label="IP address">{{ systemInfo?.ip_address }}</InfoRowComponent>
      </div>

      <!-- CPU -->
      <div>
        <h2 class="font-bold mb-2">CPU</h2>
        <InfoRowComponent label="CPU Usage">{{ systemInfo?.cpu }}</InfoRowComponent>
        <InfoRowComponent label="CPU Temperature">{{ systemInfo?.temperature }}</InfoRowComponent>
      </div>

      <!-- RAM -->
      <div>
        <h2 class="font-bold mb-2">Memory (RAM)</h2>
        <InfoRowComponent label="Total RAM">{{ systemInfo?.ram.total }} GB</InfoRowComponent>
        <InfoRowComponent label="Used RAM">{{ systemInfo?.ram.used }} GB</InfoRowComponent>
        <InfoRowComponent label="Available RAM">{{ systemInfo?.ram.available }} GB</InfoRowComponent>
      </div>

      <!-- Disk -->
      <div>
        <h2 class="font-bold mb-2">Disk</h2>
        <InfoRowComponent label="Total Disk">{{ systemInfo?.disk.total }} GB</InfoRowComponent>
        <InfoRowComponent label="Used Disk">{{ systemInfo?.disk.used }} GB</InfoRowComponent>
        <InfoRowComponent label="Free Disk">{{ systemInfo?.disk.free }} GB</InfoRowComponent>
      </div>

      <!-- System Info -->
      <div>
        <h2 class="font-bold mb-2">System</h2>
        <InfoRowComponent label="Operating System">{{ systemInfo?.os }}</InfoRowComponent>
        <InfoRowComponent label="System Uptime">{{ systemInfo?.uptime }}</InfoRowComponent>
      </div>

      <!-- Wi-Fi -->
      <div>
        <h2 class="font-bold mb-2">Wi-Fi</h2>
        <InfoRowComponent label="Wi-Fi SSID">{{ systemInfo?.wifi.ssid }}</InfoRowComponent>
        <InfoRowComponent label="Wi-Fi Signal">{{ systemInfo?.wifi.signal }}</InfoRowComponent>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'
import PageHeader from '@/components/PageHeader.vue'
import LoadingSpinner from '@/components/LoadingSpinner.vue'
import InfoRowComponent from '@/components/InfoRowComponent.vue'
import { useWebSocketStore } from '@/stores/useWebsocketStore'
import { WsTopicBuilder } from '@/utils/ws'

// Типизация system_info (можно потом вынести в отдельный файл types)
interface SystemInfo {
  host_name: string
  ip_address: string
  app_version: string
  cpu: string
  ram: { total: number; used: number; available: number }
  disk: { total: number; used: number; free: number }
  os: string
  uptime: string
  temperature: string
  wifi: { ssid: string; signal: string }
  [key: string]: any
}

const wsStore = useWebSocketStore()

const topicSystemInfo = WsTopicBuilder.systemInfo()

// Локальное состояние
const systemInfo = ref<SystemInfo | null>(null)

// Callback при получении "system_info"
const handleSystemInfo = (data: SystemInfo) => {
  systemInfo.value = data
}

// Подписка при монтировании
onMounted(() => {
  wsStore.subscribe([topicSystemInfo])
  wsStore.subscribeToChannel(topicSystemInfo, handleSystemInfo)
})

// Отписка при размонтировании
onUnmounted(() => {
  wsStore.unsubscribe([topicSystemInfo])
  wsStore.unsubscribeFromChannel(topicSystemInfo, handleSystemInfo)
})

// Проверка, что данные загружены
const isLoaded = computed(() => {
  return systemInfo?.value !== null
})
</script>
