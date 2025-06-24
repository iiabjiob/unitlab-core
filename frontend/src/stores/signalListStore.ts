import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { SignalListEntry } from '@/types/signalListEntry'

export const useSignalListStore = defineStore('signalListStore', () => {
  const signals = ref<SignalListEntry[]>([])

  // Загрузка с backend
  async function fetchSignalList() {

  }

  return {
    signals,
    fetchSignalList,
  }
})
