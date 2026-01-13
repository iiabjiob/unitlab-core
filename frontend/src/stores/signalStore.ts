import { defineStore } from "pinia"
import { computed, reactive, ref } from "vue"

import { SignalsAPI } from "@/api/signals.api"
import type { Signal, SignalCreatePayload, SignalUpdatePayload } from "@/types/signal"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { getLogger } from "@/utils/logger"

const logger = getLogger("LIVE_SIGNALS")

export const useSignalsStore = defineStore("signalsStore", () => {
  const workspaceStore = useWorkspaceStore()
  const signals = ref<Signal[]>([])
  const signalDetails = reactive<Record<number, Signal>>({})
  const loading = ref(false)
  const initializedWorkspaceId = ref<number | null>(null)

  function upsertSignal(signal: Signal) {
    signalDetails[signal.id] = signal
    const idx = signals.value.findIndex(item => item.id === signal.id)
    if (idx === -1) {
      signals.value = [signal, ...signals.value]
    } else {
      signals.value[idx] = signal
    }
  }

  function removeSignal(signalId: number) {
    signals.value = signals.value.filter(item => item.id !== signalId)
    delete signalDetails[signalId]
  }

  async function refreshSignals(force = false) {
    const workspaceId = workspaceStore.requireWorkspaceId()
    if (!force && signals.value.length && initializedWorkspaceId.value === workspaceId) {
      return
    }
    loading.value = true
    try {
      const { data } = await SignalsAPI.list(workspaceId)
      signals.value = data
      initializedWorkspaceId.value = workspaceId
      data.forEach(signal => {
        signalDetails[signal.id] = signal
      })
      logger.debug("🔁 Loaded", data.length, "signals")
    } finally {
      loading.value = false
    }
  }

  async function getSignal(signalId: number, force = false) {
    if (!force) {
      const cached = signalDetails[signalId]
      if (cached) return cached
      const fromList = signals.value.find(signal => signal.id === signalId)
      if (fromList) {
        signalDetails[signalId] = fromList
        return fromList
      }
    }
    const { data } = await SignalsAPI.get(signalId)
    upsertSignal(data)
    return data
  }

  async function createSignal(payload: SignalCreatePayload) {
    const workspaceId = workspaceStore.requireWorkspaceId()
    const { data } = await SignalsAPI.create(workspaceId, payload)
    upsertSignal(data)
    return data
  }

  async function updateSignal(signalId: number, payload: SignalUpdatePayload) {
    const { data } = await SignalsAPI.update(signalId, payload)
    upsertSignal(data)
    return data
  }

  async function deleteSignal(signalId: number) {
    await SignalsAPI.delete(signalId)
    removeSignal(signalId)
  }

  const activeSignals = computed(() => signals.value.filter(signal => signal.is_active))
  const inactiveSignals = computed(() => signals.value.filter(signal => !signal.is_active))

  return {
    signals,
    signalDetails,
    loading,
    activeSignals,
    inactiveSignals,
    refreshSignals,
    getSignal,
    createSignal,
    updateSignal,
    deleteSignal,
  }
})
