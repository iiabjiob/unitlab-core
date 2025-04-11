<template>
  <div>
    <PageHeader title="System Information" />

    <!-- Показываем спинер, пока не загружены все ключевые данные -->
    <LoadingSpinner size="small" position="left" v-if="!isLoaded" />

    <!-- Когда данные готовы, показываем всю страницу -->
    <div v-else class="space-y-3 text-sm">

      <!-- APP Version -->
      <div>
        <InfoRowComponent label="Version">{{ systemInfo.app_version }}</InfoRowComponent>
      </div>

      <!-- Network -->
      <div>
        <h2 class="font-bold mb-2">Network</h2>
        <InfoRowComponent label="Host">{{ systemInfo.host_name }}</InfoRowComponent>
        <InfoRowComponent label="IP address">{{ systemInfo.ip_address }}</InfoRowComponent>
      </div>

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

<script setup lang="ts">
import { computed, onMounted, onUnmounted } from 'vue'
import PageHeader from '@/components/PageHeader.vue'
import LoadingSpinner from '@/components/LoadingSpinner.vue'
import { useWebSocketStore } from '@/stores/websocket'
import InfoRowComponent from '@/components/InfoRowComponent.vue'

// Типизация структуры system_info (можно вынести в /types)
interface SystemInfo {
  host_name: string
  ip_address: string
  cpu: string
  ram: string
  disk: string
  os: string
  uptime: string
  temperature: string
  wifi: string
  [key: string]: any // если могут быть доп. поля
}

const wsStore = useWebSocketStore()

// 📡 Подписка/отписка
onMounted(() => {
  wsStore.subscribe(['system_info'])
})

onUnmounted(() => {
  wsStore.unsubscribe(['system_info'])
})

// 🧠 Данные с проверкой
const systemInfo = computed<SystemInfo | Record<string, any>>(() =>
  wsStore.receivedData['system_info'] ?? {}
)

// ✅ Готовность к отображению
const isLoaded = computed<boolean>(() => {
  const info = systemInfo.value
  return (
    !!info.host_name &&
    !!info.ip_address &&
    !!info.cpu &&
    !!info.ram &&
    !!info.disk &&
    !!info.os &&
    !!info.uptime &&
    !!info.temperature &&
    !!info.wifi
  )
})
</script>
