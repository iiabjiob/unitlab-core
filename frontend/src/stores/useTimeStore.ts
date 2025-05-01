import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import { useWebSocketStore } from './useWebsocketStore'
import { WsTopicBuilder } from '@/utils/ws'

export const useTimeStore = defineStore('timeStore', () => {
  const wsStore       = useWebSocketStore()
  const topic         = WsTopicBuilder.timeStatus()
  const syncBaseTime  = ref<Date|null>(null)
  const syncStartTime = ref<number>(0)

  // Handler при получении сообщения
  function handle(payload: { timestamp: string; source?: string }) {
    if (payload.timestamp) {
      syncBaseTime.value  = new Date(payload.timestamp)
      syncStartTime.value = Date.now()
    }
  }

  // Подключаемся один раз
  wsStore.subscribe([topic])
  wsStore.subscribeToChannel(topic, handle)

  // Тик для обновления каждые 1с
  const now = ref<number>(Date.now())
  setInterval(() => { now.value = Date.now() }, 1000)

  const formatted = computed(() => {
    if (syncBaseTime.value) {
      const elapsed = now.value - syncStartTime.value
      return new Date(syncBaseTime.value.getTime() + elapsed)
        .toLocaleString()
    }
    return new Date().toLocaleString()
  })

  const sourceLabel = computed(() => {
    const s = wsStore.receivedData['time_status']?.source
    switch (s) {
      case 'NTP': return 'NTP'
      case 'PTP': return 'PTP'
      default:    return 'Local'
    }
  })

  return { formatted, sourceLabel }
})
