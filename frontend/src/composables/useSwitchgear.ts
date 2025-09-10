// src/composables/useSwitchgear.ts
import { computed, ref, watch } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useWebSocketStore } from "@/stores/websocketStore"

export type SwitchgearState = "CLOSED" | "OPEN" | "UNKNOWN" | "INTERMEDIATE"

const PAIR_CODE: Record<SwitchgearState, 0 | 1 | 2 | 3> = {
  UNKNOWN: 0b00,
  OPEN: 0b01,
  CLOSED: 0b10,
  INTERMEDIATE: 0b11,
}

export interface UseSwitchgearOpts {
  doUnitId?: string
  doOpenCh: number
  doCloseCh: number
  diUnitId?: string
  diOpenPulseCh: number
  diClosePulseCh: number
}

export function useSwitchgear(opts: UseSwitchgearOpts) {
  const deviceStore = useDeviceStore()
  const channelStore = useChannelStore()
  const ws = useWebSocketStore()

  // Busy flag + pending target highlight
  const busy = ref(false)
  const pendingTarget = ref<SwitchgearState | null>(null)

  // Optional UI delay before sending a DO pair
  const feedbackDelayMs = ref(0)

  // Resolve unit ids if not provided
  const doUnitId = computed(() => {
    if (opts.doUnitId) return opts.doUnitId
    return deviceStore.devices.find(d => d.type?.toLowerCase() === "do")?.unit_id ?? "unknown-do"
  })
  const diUnitId = computed(() => {
    if (opts.diUnitId) return opts.diUnitId
    return deviceStore.devices.find(d => d.type?.toLowerCase() === "di")?.unit_id ?? "unknown-di"
  })

  // Helper to read channel boolean state
  function getChannelState(unitId: string, ch: number, expectedType: "DO" | "DI"): boolean | null {
    const dev = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!dev || dev.type !== expectedType) return null
    const arr = channelStore.channels[unitId]
    const c = arr?.find(x => x.index === ch)
    return typeof (c as any)?.state === "boolean" ? (c as any).state as boolean : null
  }
  const getDo = (id: string, ch: number) => getChannelState(id, ch, "DO")
  const getDi = (id: string, ch: number) => getChannelState(id, ch, "DI")

  // Effective state is determined by the DO pair only
  const effectiveState = computed<SwitchgearState>(() => {
    const a = getDo(doUnitId.value, opts.doOpenCh)
    const b = getDo(doUnitId.value, opts.doCloseCh)
    if (a === null || b === null) return "UNKNOWN"
    if (a && !b) return "OPEN"
    if (!a && b) return "CLOSED"
    if (!a && !b) return "UNKNOWN"
    if (a && b) return "INTERMEDIATE"
    return "UNKNOWN"
  })

  // Clear pending when feedback matches target
  watch(effectiveState, (cur) => {
    if (pendingTarget.value && cur === pendingTarget.value) pendingTarget.value = null
  })

  // Guard for buttons
  const isCmdDisabled = (target: SwitchgearState) => {
    if (!ws.isConnected || busy.value) return true
    return effectiveState.value === target
  }

  function sendDoPair(target: SwitchgearState) {
    if (doUnitId.value.startsWith("unknown")) return
    channelStore.sendDoPairCommand(
      doUnitId.value,
      opts.doOpenCh,
      opts.doCloseCh,
      PAIR_CODE[target],
    )
  }

  async function setDoPair(target: SwitchgearState) {
    if (doUnitId.value.startsWith("unknown")) return
    pendingTarget.value = target
    busy.value = true
    try {
      sendDoPair(target)
    } finally {
      setTimeout(() => (busy.value = false), 120)
    }
  }

  function scheduleDoPair(target: SwitchgearState) {
    if (doUnitId.value.startsWith("unknown")) return
    pendingTarget.value = target
    busy.value = true
    setTimeout(() => {
      sendDoPair(target)
      busy.value = false
    }, feedbackDelayMs.value)
  }

  // DI rising-edge → schedule respective DO pair
  const lastDiOpen = ref<boolean | null>(null)
  const lastDiClose = ref<boolean | null>(null)

  watch(
    () => ({
      diOpen: getDi(diUnitId.value, opts.diOpenPulseCh),
      diClose: getDi(diUnitId.value, opts.diClosePulseCh),
    }),
    ({ diOpen, diClose }) => {
      if (diOpen !== null) {
        if (lastDiOpen.value === false && diOpen === true) scheduleDoPair("OPEN")
        lastDiOpen.value = diOpen
      }
      if (diClose !== null) {
        if (lastDiClose.value === false && diClose === true) scheduleDoPair("CLOSED")
        lastDiClose.value = diClose
      }
    }
  )

  return {
    // state
    effectiveState, pendingTarget, busy, feedbackDelayMs,
    doUnitId, diUnitId,
    // actions
    isCmdDisabled, setDoPair,
  }
}
