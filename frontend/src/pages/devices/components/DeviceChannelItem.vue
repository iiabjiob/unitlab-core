<template>
  <div
    class="group flex items-center justify-between px-2 py-1.5 text-sm select-none"
    :class="{ 'opacity-60': disabled }"
  >
    <div class="flex items-center gap-3">

      <!-- DO control -->
      <div
        v-if="effectiveType === 'do'"
        class="w-4 h-4 rounded-sm border cursor-default flex items-center justify-center
               transition-colors"
        :class="[doControlClass, { 'cursor-not-allowed opacity-70': isWaiting || disabled }]"
        @click.stop="onToggleClick"
      >
        <span
          v-if="isWaiting"
          class="w-2.5 h-2.5 border-[1.5px] border-white/80 border-t-transparent rounded-full animate-spin"
        />
        <span v-else-if="isError" class="text-[10px] font-semibold text-white">!</span>
        <span v-else-if="channel.state" class="text-[10px] text-white">✓</span>
      </div>

      <!-- DI indicator -->
      <div
        v-else-if="effectiveType === 'di'"
        class="w-4 h-4 rounded-full border transition-all duration-200"
        :class="diIndicatorClass"
      />

      <!-- AO input -->
      <div v-else-if="effectiveType === 'ao'" class="flex items-center gap-1.5">
        <input
          type="number"
          autocomplete="off"
          :id="`device-channel-ao-${channel.id}`"
          :name="`device-channel-ao-${channel.id}`"
          class="w-16 px-1 py-0.5 text-xs rounded border border-neutral-600
                 bg-neutral-900 text-neutral-200"
          :disabled="disabled"
          :value="channel.state"
          @change="onAoChange"
        />
        <span
          v-if="aoStatusClass"
          class="inline-flex items-center rounded px-1 py-0.5 text-[10px] font-medium border"
          :class="aoStatusClass"
          :title="aoStatusTitle"
        >
          {{ aoStatusLabel }}
        </span>
      </div>

      <!-- Label -->
      <div class="text-xs text-neutral-700 dark:text-neutral-300">
        {{ channel.resolved_name }}
      </div>

    </div>
  </div>
</template>


<script setup lang="ts">
import { computed } from "vue"
import type { AoChannel, Channel, DiChannel, DoChannel } from "@/types/channel"

const props = withDefaults(defineProps<{
  channel: Channel
  disabled?: boolean
  deviceType?: "do" | "di" | "ao"
}>(), {
  disabled: false,
  deviceType: undefined,
})

const emit = defineEmits<{
  (e: "toggle", ch: Channel): void
  (e: "set-ao", payload: { channel: Channel; value: number }): void
}>()

const effectiveType = computed<Channel["type"] | null>(() => {
  const channelType = props.channel.type
  const expectedType = props.deviceType
  if (!expectedType) {
    return channelType
  }
  return channelType === expectedType ? channelType : null
})

const status = computed(() =>
  doChannel.value?.ui?.stage ?? "idle"
)
const isWaiting = computed(() => status.value === "pending" || status.value === "debounce")
const isError = computed(() => status.value === "error")

const doChannel = computed<DoChannel | null>(() => (
  effectiveType.value === "do" && props.channel.type === "do" ? props.channel : null
))

const diChannel = computed<DiChannel | null>(() => (
  effectiveType.value === "di" && props.channel.type === "di" ? props.channel : null
))

const aoChannel = computed<AoChannel | null>(() => (
  effectiveType.value === "ao" && props.channel.type === "ao" ? props.channel : null
))

const diAlertActive = computed(() => {
  const diag = diChannel.value?.diDiagnostics
  if (!diag) {
    return false
  }
  return Boolean(diag.stuck || diag.lost || diag.latched)
})

const diIndicatorClass = computed(() => {
  if (effectiveType.value !== "di") {
    return ""
  }
  if (diAlertActive.value) {
    return "bg-red-500 border-red-500 animate-pulse"
  }
  return props.channel.state
    ? "bg-green-500 border-green-600"
    : "bg-neutral-600 border-neutral-500"
})

const doControlClass = computed(() => {
  if (effectiveType.value !== "do") {
    return ""
  }
  if (isError.value) {
    return "bg-red-500 border-red-500 text-white animate-pulse"
  }
  if (doChannel.value?.state) {
    return "bg-green-500 border-green-600 text-white"
  }
  if (isWaiting.value) {
    return "bg-yellow-400/80 border-yellow-500 text-yellow-900"
  }
  return "bg-neutral-300 dark:bg-neutral-700 border-neutral-600 hover:bg-neutral-600"
})

const aoStatus = computed(() => aoChannel.value?.diagnostics?.quality)
const aoHasError = computed(() => Boolean(aoChannel.value?.diagnostics?.hasError))

const aoStatusLabel = computed(() => {
  if (aoStatus.value === "valid") return "OK"
  if (aoStatus.value === "pending") return "PEND"
  if (aoStatus.value === "fault") return "FAULT"
  return ""
})

const aoStatusClass = computed(() => {
  if (!aoStatus.value) {
    return ""
  }
  if (aoStatus.value === "valid") {
    return aoHasError.value
      ? "border-amber-500 text-amber-200 bg-amber-900/40"
      : "border-emerald-500 text-emerald-200 bg-emerald-900/40"
  }
  if (aoStatus.value === "pending") {
    return "border-yellow-500 text-yellow-100 bg-yellow-900/50 animate-pulse"
  }
  return "border-red-500 text-red-100 bg-red-900/50 animate-pulse"
})

const aoStatusTitle = computed(() => {
  if (!aoStatus.value) {
    return "AO diagnostics unavailable"
  }
  if (aoHasError.value) {
    return `AO ${aoStatus.value.toUpperCase()} (backend error latched)`
  }
  return `AO ${aoStatus.value.toUpperCase()}`
})

function onToggleClick() {
  if (effectiveType.value !== "do" || isWaiting.value || props.disabled) {
    return
  }
  emit("toggle", props.channel)
}

function onAoChange(event: Event) {
  if (props.disabled) {
    return
  }
  const raw = (event.target as HTMLInputElement).value
  const num = Number(raw)
  if (!isNaN(num)) {
    emit("set-ao", { channel: props.channel, value: num })
  }
}
</script>
