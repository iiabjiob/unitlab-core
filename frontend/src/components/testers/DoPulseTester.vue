<template>
  <div class="flex flex-col gap-2 p-3 rounded-xl bg-white dark:bg-neutral-800 shadow-sm border border-neutral-200 dark:border-neutral-700 w-[300px]">
    <div class="flex items-center justify-between">
      <div class="text-sm font-medium text-neutral-800 dark:text-neutral-100">{{ title }}</div>
    </div>

    <div class="grid grid-cols-2 gap-2">
      <ButtonComponent type="secondary"
        :disabled="!ws.isConnected || isBusyA"
        @click="pulseA"
        :title="`Pulse ${pulseMs}ms on CH${chA}`"
      >
        {{ labelA }}
      </ButtonComponent>

      <ButtonComponent type="secondary"
        :disabled="!ws.isConnected || isBusyB"
        @click="pulseB"
        :title="`Pulse ${pulseMs}ms on CH${chB}`"
      >
        {{ labelB }}
      </ButtonComponent>
    </div>

    <div class="text-xs text-neutral-500 dark:text-neutral-400">
      <div>DO: {{ doUnitId }} [{{ chA }}|{{ chB }}]</div>
      <div v-if="lastInfo" class="mt-1 opacity-80">{{ lastInfo }}</div>
    </div>

  </div>
</template>

<script setup lang="ts">
// All comments are in English.

import { computed, ref } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useWebSocketStore } from "@/stores/websocketStore"
import ButtonComponent from "../ui/ButtonComponent.vue";

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
