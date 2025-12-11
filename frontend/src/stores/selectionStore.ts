import { defineStore } from "pinia"
import { ref } from "vue"
import { useRouter } from "vue-router"

type SelectionState = {
  lastDeviceId: number | null
  lastSwitchgearId: number | null
  lastSequenceId: number | null
}

const STORAGE_KEY = "unitlab.selection"

export const useSelectionStore = defineStore("selection", () => {

  // --- STATE ---------------------------------------------------------
  const lastDeviceId = ref<number | null>(null)
  const lastSwitchgearId = ref<number | null>(null)
  const lastSequenceId = ref<number | null>(null)

  // --- PERSISTENCE ---------------------------------------------------
  function persist() {
    localStorage.setItem(
      STORAGE_KEY,
      JSON.stringify({
        lastDeviceId: lastDeviceId.value,
        lastSwitchgearId: lastSwitchgearId.value,
        lastSequenceId: lastSequenceId.value,
      })
    )
  }

  function restore() {
    const raw = localStorage.getItem(STORAGE_KEY)
    if (!raw) return

    try {
      const data: SelectionState = JSON.parse(raw)

      lastDeviceId.value = data.lastDeviceId ?? null
      lastSwitchgearId.value = data.lastSwitchgearId ?? null
      lastSequenceId.value = data.lastSequenceId ?? null
    } catch (err) {
      console.warn("Failed to restore selection:", err)
    }
  }

  // --- ACTIONS -------------------------------------------------------

  function selectDevice(id: number) {
    lastDeviceId.value = id
    persist()
  }

  function selectSwitchgear(id: number) {
    lastSwitchgearId.value = id
    persist()
  }

  function selectSequence(id: number) {
    lastSequenceId.value = id
    persist()
  }

  // --- NAVIGATION ----------------------------------------------------
  function openLast(router = useRouter()) {
    // priority: most recently used page → pick whichever exists
    if (lastSequenceId.value) {
      router.push(`/sequences/${lastSequenceId.value}`)
      return
    }
    if (lastDeviceId.value) {
      router.push(`/devices/${lastDeviceId.value}`)
      return
    }
    if (lastSwitchgearId.value) {
      router.push(`/switchgears/${lastSwitchgearId.value}`)
      return
    }

    // default
    router.push("/")
  }

  return {
    // state
    lastDeviceId,
    lastSequenceId,
    lastSwitchgearId,

    // actions
    selectDevice,
    selectSequence,
    selectSwitchgear,

    // persistence
    restore,
    persist,

    // navigation
    openLast,
  }
})
