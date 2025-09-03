<template>
  <div
    class="flex flex-col gap-3 p-4 rounded-xl bg-white dark:bg-neutral-800 shadow-md border border-neutral-200 dark:border-neutral-700 w-[300px]">
    <!-- Header -->
    <div class="flex items-center justify-between">
      <div class="text-sm font-medium text-neutral-700 dark:text-neutral-200">
        {{ title }}
      </div>
      <span class="text-xs px-2 py-0.5 rounded-full" :class="statePillClass">
        {{ effectiveState }}
      </span>
    </div>

    <!-- Visual cube driven by DO feedback only -->
    <div
      class="h-28 w-28 mx-auto rounded-lg transition-all duration-200 border-2 flex items-center justify-center select-none relative"
      :class="cubeClass" :title="`State (by DO): ${effectiveState}`">
      <template v-if="effectiveState === 'UNKNOWN'">
        <span class="text-3xl font-bold">?</span>
      </template>
      <template v-else-if="effectiveState === 'INTERMEDIATE'">
        <div class="relative w-full h-full flex items-center justify-center">
          <div class="absolute rotate-45 w-[140%] h-[3px] bg-current opacity-80"></div>
        </div>
      </template>

      <!-- Pending target hint -->
      <div v-if="pendingTarget !== null" class="pointer-events-none absolute inset-0 rounded-lg border-2 animate-pulse"
        :class="pendingBorderClass" />
    </div>

    <!-- Buttons (send commands only) -->
    <div class="grid grid-cols-2 gap-2">
      <ButtonComponent type="secondary" :disabled="isCmdDisabled('OPEN')" @click="cmdOpen">
        Open
      </ButtonComponent>
      <ButtonComponent type="secondary" :disabled="isCmdDisabled('CLOSED')" @click="cmdClose">
        Close
      </ButtonComponent>
      <ButtonComponent type="secondary" :disabled="isCmdDisabled('UNKNOWN')" @click="cmdUnknown">
        Unknown
      </ButtonComponent>
      <ButtonComponent type="secondary" :disabled="isCmdDisabled('INTERMEDIATE')" @click="cmdIntermediate">
        Intermediate
      </ButtonComponent>
    </div>

    <!-- Tech footer -->
    <div class="text-xs text-neutral-500 dark:text-neutral-400">
      <div>DO: {{ doUnitId }} [{{ doOpenCh }}|{{ doCloseCh }}]</div>
      <div>DI: {{ diUnitId }} [{{ diOpenPulseCh }}|{{ diClosePulseCh }}]</div>
      <div class="flex items-center gap-2">
        <span>Feedback delay:</span>
        <input name="feedbackDelayMs" type="number" min="0" step="100" v-model.number="feedbackDelayMs" class="w-16 px-1 py-0.5 text-xs rounded border border-neutral-300 dark:border-neutral-600
             bg-white dark:bg-neutral-700 text-neutral-700 dark:text-neutral-200" />
        <span>ms</span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">

import { computed, onMounted, ref, watch } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useWebSocketStore } from "@/stores/websocketStore"
import ButtonComponent from "./ui/ButtonComponent.vue"

type SwitchgearState = "CLOSED" | "OPEN" | "UNKNOWN" | "INTERMEDIATE"

// 2-bit mapping for pair state (mirrors backend mask 0x03):
//   00 -> UNKNOWN
//   01 -> OPEN
//   10 -> CLOSED
//   11 -> INTERMEDIATE
const PAIR_CODE: Record<SwitchgearState, 0 | 1 | 2 | 3> = {
  UNKNOWN: 0b00,
  OPEN: 0b01,
  CLOSED: 0b10,
  INTERMEDIATE: 0b11,
}

const props = withDefaults(defineProps<{
  title?: string
  doUnitId?: string
  doOpenCh?: number
  doCloseCh?: number
  diUnitId?: string
  diOpenPulseCh?: number
  diClosePulseCh?: number
}>(), {
  title: "2-Pos Switchgear",
  doOpenCh: 0,
  doCloseCh: 1,
  diOpenPulseCh: 0,
  diClosePulseCh: 1,
})

const deviceStore = useDeviceStore()
const channelStore = useChannelStore()
const ws = useWebSocketStore()

const busy = ref(false)
const pendingTarget = ref<SwitchgearState | null>(null)

const doUnitId = computed(() => {
  if (props.doUnitId) return props.doUnitId
  return deviceStore.devices.find(d => d.type?.toLowerCase() === "do")?.unit_id ?? "unknown-do"
})
const diUnitId = computed(() => {
  if (props.diUnitId) return props.diUnitId
  return deviceStore.devices.find(d => d.type?.toLowerCase() === "di")?.unit_id ?? "unknown-di"
})

function getDoState(unitId: string, ch: number): boolean | null {
  const arr = channelStore.channels[unitId]
  if (!arr) return null
  const c = arr.find(x => x.type === "DO" && x.index === ch)
  return (c && typeof (c as any).state === "boolean") ? (c as any).state as boolean : null
}
function getDiState(unitId: string, ch: number): boolean | null {
  const arr = channelStore.channels[unitId]
  if (!arr) return null
  const c = arr.find(x => x.type === "DI" && x.index === ch)
  return (c && typeof (c as any).state === "boolean") ? (c as any).state as boolean : null
}

const isCmdDisabled = (target: SwitchgearState) => {
  // Disable if busy, disconnected, or already in that state
  if (!ws.isConnected || busy.value) return true
  return effectiveState.value === target
}

// configurable delay for feedback (ms)
const feedbackDelayMs = ref(0) // default 0ms, можно менять через UI

function scheduleDoPair(target: SwitchgearState) {
  if (doUnitId.value.startsWith("unknown")) return
  pendingTarget.value = target
  busy.value = true

  setTimeout(() => {
    channelStore.sendDoPairCommand(
      doUnitId.value,
      props.doOpenCh,
      props.doCloseCh,
      PAIR_CODE[target],
    )
    busy.value = false
  }, feedbackDelayMs.value)
}

// Derive UI state only from DO feedback:
const effectiveState = computed<SwitchgearState>(() => {
  const a = getDoState(doUnitId.value, props.doOpenCh)
  const b = getDoState(doUnitId.value, props.doCloseCh)
  if (a === null || b === null) return "UNKNOWN"
  if (a === true && b === false) return "OPEN"
  if (a === false && b === true) return "CLOSED"
  if (a === false && b === false) return "UNKNOWN"
  if (a === true && b === true) return "INTERMEDIATE"
  return "UNKNOWN"
})

// Clear pending once feedback matches target
watch(effectiveState, (cur) => {
  if (pendingTarget.value && cur === pendingTarget.value) {
    pendingTarget.value = null
  }
})

// --- Visual classes (unchanged from previous answer) ---
const cubeClass = computed(() => {
  switch (effectiveState.value) {
    case "CLOSED":
      return "bg-neutral-900 dark:bg-neutral-100 text-white dark:text-neutral-900 border-neutral-900 dark:border-neutral-100"
    case "OPEN":
      return "bg-transparent text-neutral-700 dark:text-neutral-200 border-neutral-700 dark:border-neutral-300"
    case "UNKNOWN":
      return "bg-transparent text-amber-600 dark:text-amber-400 border-amber-600/70 dark:border-amber-400/70"
    case "INTERMEDIATE":
      return "bg-transparent text-sky-600 dark:text-sky-400 border-sky-600/70 dark:border-sky-400/70"
  }
})
const statePillClass = computed(() => {
  switch (effectiveState.value) {
    case "CLOSED": return "bg-neutral-900 text-white dark:bg-neutral-100 dark:text-neutral-900"
    case "OPEN": return "bg-neutral-100 text-neutral-700 dark:bg-neutral-800 dark:text-neutral-200"
    case "UNKNOWN": return "bg-amber-100 text-amber-800 dark:bg-amber-900/40 dark:text-amber-300"
    case "INTERMEDIATE": return "bg-sky-100 text-sky-800 dark:bg-sky-900/40 dark:text-sky-300"
  }
})
const pendingBorderClass = computed(() => {
  switch (pendingTarget.value) {
    case "OPEN": return "border-sky-500/60"
    case "CLOSED": return "border-neutral-900/60 dark:border-neutral-100/60"
    case "UNKNOWN": return "border-amber-500/60"
    case "INTERMEDIATE": return "border-sky-500/60"
    default: return "border-transparent"
  }
})

async function setDoPair(expected: SwitchgearState) {
  if (doUnitId.value.startsWith("unknown")) return
  pendingTarget.value = expected
  busy.value = true
  try {
    channelStore.sendDoPairCommand(
      doUnitId.value,
      props.doOpenCh,
      props.doCloseCh,
      PAIR_CODE[expected],
    )
  } finally {
    setTimeout(() => (busy.value = false), 120)
  }
}

// Buttons -> only send commands; UI updates on DO feedback
function cmdOpen() { setDoPair("OPEN") }
function cmdClose() { setDoPair("CLOSED") }
function cmdUnknown() { setDoPair("UNKNOWN") }
function cmdIntermediate() { setDoPair("INTERMEDIATE") }

// DI pulse → drive DO via pair command, UI still from DO feedback
const lastDiOpen = ref<boolean | null>(null)
const lastDiClose = ref<boolean | null>(null)

watch(
  () => ({
    diOpen: getDiState(diUnitId.value, props.diOpenPulseCh),
    diClose: getDiState(diUnitId.value, props.diClosePulseCh),
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

onMounted(async () => {
  if (!deviceStore.devices.length) {
    await deviceStore.fetchDevices()
  }
  if (!doUnitId.value.startsWith("unknown")) {
    channelStore.requestStates(doUnitId.value, "DO")
  }
  if (!diUnitId.value.startsWith("unknown")) {
    channelStore.requestStates(diUnitId.value, "DI")
  }
})
</script>
