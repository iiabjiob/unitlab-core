<template>
  <div class="flex flex-wrap text-xs gap-x-2 border border-gray-300 dark:border-gray-700 rounded px-2 py-0.5 text-gray-600 dark:text-gray-400">
    <span>{{ timeSourceLabel }}</span>
    <span class="tabular-nums font-mono text-right">{{ formattedTime }}</span>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useWebSocketStore } from '@/stores/websocket'

const wsStore = useWebSocketStore()

// 🕒 Local time
const localTime = ref<Date>(new Date())

// 🕒 Synchronized time (received via WebSocket)
const syncBaseTime = ref<Date | null>(null)
const syncStartTime = ref<Date | null>(null)

// ⏱ Update local clock every second
onMounted(() => {
  wsStore.subscribe(['time_status'])

  setInterval(() => {
    localTime.value = new Date()
  }, 1000)
})

// 📡 Watch for time_status messages
watch(
  () => wsStore.receivedData['time_status'],
  (newData: { timestamp?: string; source?: string } | undefined) => {
    if (newData?.timestamp) {
      syncBaseTime.value = new Date(newData.timestamp)
      syncStartTime.value = new Date()
    }
  },
  { immediate: true }
)

// 🧭 Time source (LOCAL / NTP / PTP)
const timeSource = computed<'LOCAL' | 'NTP' | 'PTP'>(() => {
  if (!wsStore.isConnected) return 'LOCAL'
  return (wsStore.receivedData['time_status']?.source as 'LOCAL' | 'NTP' | 'PTP') ?? 'LOCAL'
})

// ⏳ Force update for computed every second
const tick = ref<number>(0)
setInterval(() => tick.value++, 1000)

// 📅 Final time to show
const formattedTime = computed<string>(() => {
  tick.value // trigger reactivity

  if (syncBaseTime.value && syncStartTime.value) {
    const now = new Date()
    const elapsed = now.getTime() - syncStartTime.value.getTime()
    return new Date(syncBaseTime.value.getTime() + elapsed).toLocaleString()
  }

  return localTime.value.toLocaleString()
})

// 📛 Human-readable label
const sourceLabels: Record<'LOCAL' | 'NTP' | 'PTP', string> = {
  LOCAL: 'Local Time',
  NTP: 'NTP',
  PTP: 'PTP'
}

const timeSourceLabel = computed<string>(() => sourceLabels[timeSource.value] ?? 'Unknown')
</script>
