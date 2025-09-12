// src/composables/useSwitchgear.ts
import { computed, ref, watch } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useWebSocketStore } from "@/stores/websocketStore"
import { codeToState, SWITCHGEAR_CODE } from "@/constants/switchgear"

export type SwitchgearState = "CLOSED" | "OPEN" | "UNKNOWN" | "INTERMEDIATE"

export interface UseSwitchgearOpts {
  doOpen:   { unitId: string; channel: number } | null
  doClosed: { unitId: string; channel: number } | null
  diOpen:   { unitId: string; channel: number } | null
  diClose:  { unitId: string; channel: number } | null
  feedbackDelayMs?: number
}

export function useSwitchgear(opts: UseSwitchgearOpts) {
  const deviceStore = useDeviceStore()
  const channelStore = useChannelStore()
  const ws = useWebSocketStore()

  // Busy flag + pending target highlight
  const busy = ref(false)
  const pendingTarget = ref<SwitchgearState | null>(null)

  const feedbackDelayMs = ref(opts.feedbackDelayMs ?? 0)

  // --- Helpers ---
  function getChannelState(
    channel: { unitId: string; channel: number } | null,
    expectedType: "do" | "di"
  ): boolean | null {
    if (!channel) return null
    const dev = deviceStore.devices.find(d => d.unit_id === channel.unitId)
    if (!dev || dev.type !== expectedType) return null

    const arr = channelStore.channels.filter(c => c.device_id === dev.id)
    const c = arr.find(x => x.index === channel.channel)

    return typeof (c as any)?.state === "boolean" ? (c as any).state as boolean : null
  }

  const getDo = (channel: { unitId: string; channel: number } | null) =>
    getChannelState(channel, "do")
  const getDi = (channel: { unitId: string; channel: number } | null) =>
    getChannelState(channel, "di")

  // --- Effective state from DO pair ---
  const effectiveState = computed<SwitchgearState>(() => {
    const a = getDo(opts.doOpen)
    const b = getDo(opts.doClosed)
    if (a === null || b === null) return "UNKNOWN"
    return codeToState(a, b)
  })

  // Clear pending when feedback matches target
  watch(effectiveState, (cur) => {
    if (pendingTarget.value && cur === pendingTarget.value) {
      pendingTarget.value = null
    }
  })

  // Guard for buttons
  const isCmdDisabled = (target: SwitchgearState) => {
    if (!ws.isConnected || busy.value) return true
    return effectiveState.value === target
  }

  // --- Commands ---
  function sendDoPair(target: SwitchgearState) {
    if (!opts.doOpen || !opts.doClosed) return
    channelStore.sendDoPairCommand(
      opts.doOpen.unitId,
      opts.doOpen.channel,
      opts.doClosed.channel,
      SWITCHGEAR_CODE[target],
    )
  }

  async function setDoPair(target: SwitchgearState) {
    if (!opts.doOpen || !opts.doClosed) return
    pendingTarget.value = target
    busy.value = true
    try {
      sendDoPair(target)
    } finally {
      setTimeout(() => (busy.value = false), 120)
    }
  }

  function scheduleDoPair(target: SwitchgearState) {
    if (!opts.doOpen || !opts.doClosed) return
    pendingTarget.value = target
    busy.value = true
    setTimeout(() => {
      sendDoPair(target)
      busy.value = false
    }, feedbackDelayMs.value)
  }

  // --- DI → DO trigger (auto open/close) ---
  const lastDiOpen = ref<boolean | null>(null)
  const lastDiClose = ref<boolean | null>(null)

  watch(
    () => ({
      diOpen: getDi(opts.diOpen),
      diClose: getDi(opts.diClose),
    }),
    ({ diOpen, diClose }) => {
      if (diOpen !== null) {
        if (lastDiOpen.value === false && diOpen === true) {
          scheduleDoPair("OPEN")
        }
        lastDiOpen.value = diOpen
      }
      if (diClose !== null) {
        if (lastDiClose.value === false && diClose === true) {
          scheduleDoPair("CLOSED")
        }
        lastDiClose.value = diClose
      }
    }
  )

  return {
    // state
    effectiveState,
    pendingTarget,
    busy,
    feedbackDelayMs,

    // expose signals (useful for footer/debug)
    doOpen: opts.doOpen,
    doClosed: opts.doClosed,
    diOpen: opts.diOpen,
    diClose: opts.diClose,

    // actions
    isCmdDisabled,
    setDoPair,
  }
}
