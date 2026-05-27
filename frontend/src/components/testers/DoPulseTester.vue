<template>
  <div class="do-pulse-tester">
    <div class="do-pulse-tester__header">
      <div class="do-pulse-tester__title">{{ title }}</div>
    </div>

    <div class="do-pulse-tester__actions">
      <UiButton
        type="button"
        variant="secondary"
        :disabled="!ws.isConnected || isBusyA"
        @click="pulseA"
        :title="`Pulse ${pulseMs}ms on CH${chA}`"
      >
        {{ labelA }}
      </UiButton>

      <UiButton
        type="button"
        variant="secondary"
        :disabled="!ws.isConnected || isBusyB"
        @click="pulseB"
        :title="`Pulse ${pulseMs}ms on CH${chB}`"
      >
        {{ labelB }}
      </UiButton>
    </div>

    <div class="do-pulse-tester__meta">
      <div>DO: {{ doUnitId }} [{{ chA }}|{{ chB }}]</div>
      <div v-if="lastInfo" class="do-pulse-tester__last-info">{{ lastInfo }}</div>
    </div>

  </div>
</template>

<script setup lang="ts">
// All comments are in English.

import { computed, ref } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useWebSocketStore } from "@/stores/websocketStore"
import UiButton from "../ui/UiButton.vue"

const props = withDefaults(defineProps<{
  title?: string
  doUnitId?: string
  /** Default channels for pulses (wired to switchgear inputs) */
  chA?: number
  chB?: number
  /** Pulse width in ms */
  pulseMs?: number
  /** Labels for ButtonComponents */
  labelA?: string
  labelB?: string
}>(), {
  title: "DO Pulse Tester",
  chA: 4,
  chB: 5,
  pulseMs: 200,
  labelA: "Pulse A",
  labelB: "Pulse B",
})

const deviceStore = useDeviceStore()
const channelStore = useChannelStore()
const ws = useWebSocketStore()

// Resolve DO unit id from props or pick the first DO device.
const doUnitResolved = computed(() => {
  if (props.doUnitId) return props.doUnitId
  return deviceStore.devices.find(d => d.type?.toLowerCase() === "do")?.unit_id ?? "unknown-do"
})

// Per-channel busy flags to prevent spamming during the pulse window.
const isBusyA = ref(false)
const isBusyB = ref(false)
const lastInfo = ref<string | null>(null)

// Send a single-bit ON then schedule OFF after pulseMs.
// NOTE: For true hardware-level pulses, implement server-side pulse and expose a dedicated command.
// This client-side approach is adequate for quick tests.
function pulseChannel(ch: number, busyRef: typeof isBusyA) {
  const unitId = doUnitResolved.value
  if (unitId.startsWith("unknown")) {
    lastInfo.value = "No DO device available."
    return
  }
  if (busyRef.value) return
  busyRef.value = true

  // ON
  channelStore.sendDoCommand(unitId, ch, true)
  lastInfo.value = `CH${ch}: ON → ${props.pulseMs}ms → OFF`

  // OFF after pulseMs
  setTimeout(() => {
    channelStore.sendDoCommand(unitId, ch, false)
    busyRef.value = false
  }, props.pulseMs)
}

function pulseA() { pulseChannel(props.chA, isBusyA) }
function pulseB() { pulseChannel(props.chB, isBusyB) }
</script>

<style scoped>
.do-pulse-tester {
  display: flex;
  width: 300px;
  flex-direction: column;
  gap: 0.5rem;
  padding: 0.75rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: 0.75rem;
  background: var(--color-white);
  box-shadow: var(--shadow-sm);
}

.do-pulse-tester__header {
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.do-pulse-tester__title {
  color: var(--color-neutral-800);
  font-size: var(--text-sm);
  font-weight: 500;
}

.do-pulse-tester__actions {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 0.5rem;
}

.do-pulse-tester__meta {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.do-pulse-tester__last-info {
  margin-top: 0.25rem;
  opacity: 0.8;
}

:global(.dark .do-pulse-tester) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-800);
}

:global(.dark .do-pulse-tester__title) {
  color: var(--color-neutral-100);
}

:global(.dark .do-pulse-tester__meta) {
  color: var(--color-neutral-400);
}
</style>
