// stores/timeStore.ts
import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

export const useTimeStore = defineStore('timeStore', () => {
  const timestamp = ref<string | null>(null)
  const source = ref<string | null>(null)

  function updateFromSync(event: { timestamp: string; source?: string }) {
    timestamp.value = event.timestamp
    source.value = event.source ?? null
  }

  const formatted = computed(() => {
    if (!timestamp.value) return '--'
    try {
      const date = new Date(timestamp.value)
      return new Intl.DateTimeFormat('en-GB', {
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
      }).format(date)
    } catch {
      return '--'
    }
  })

  const sourceLabel = computed(() => source.value ?? 'unsynced')

  return { timestamp, source, formatted, sourceLabel, updateFromSync }
})
