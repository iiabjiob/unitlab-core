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
  let restored = false

  function normalizeId(id: number | null | undefined): number | null {
    return typeof id === "number" && Number.isFinite(id) ? id : null
  }

  function setSelection(target: { value: number | null }, id: number | null | undefined) {
    const next = normalizeId(id)
    if (target.value === next) return
    target.value = next
    persist()
  }

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
    if (restored) return
    const raw = localStorage.getItem(STORAGE_KEY)
    if (!raw) {
      restored = true
      return
    }

    try {
      const data: SelectionState = JSON.parse(raw)

      lastDeviceId.value = data.lastDeviceId ?? null
      lastSwitchgearId.value = data.lastSwitchgearId ?? null
      lastSequenceId.value = data.lastSequenceId ?? null
    } catch (err) {
      console.warn("Failed to restore selection:", err)
    } finally {
      restored = true
    }
  }

  // --- ACTIONS -------------------------------------------------------

  function selectDevice(id: number | null) {
    setSelection(lastDeviceId, id)
  }

  function selectSwitchgear(id: number | null) {
    setSelection(lastSwitchgearId, id)
  }

  function selectSequence(id: number | null) {
    setSelection(lastSequenceId, id)
  }

  // --- NAVIGATION ----------------------------------------------------
  function openLast(router = useRouter()) {
    // priority: most recently used page → pick whichever exists
    if (lastSequenceId.value) {
      router.push(`/test-runs/instructions/${lastSequenceId.value}`)
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
