<template>
  <div
    class="group flex justify-center px-2 py-1.5 text-sm select-none"
    :class="{ 'opacity-60': disabled }"
  >
    <div class="grid w-full max-w-[40rem] min-w-0 grid-cols-[6.5rem_minmax(0,1fr)] items-center gap-3">

      <!-- Label -->
      <div class="flex min-w-0 items-center justify-center text-center text-xs text-neutral-700 dark:text-neutral-300">
        <span class="block min-w-0 truncate">{{ channel.resolved_name }}</span>
      </div>

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
      <div v-else-if="effectiveType === 'ao'" class="flex min-w-0 flex-1 items-center gap-2">
        <div class="flex min-w-0 items-center gap-2">
          <label
            class="flex min-w-0 flex-1 items-center gap-1.5 text-[10px] font-semibold uppercase tracking-[0.08em] text-neutral-500 dark:text-neutral-400"
            :for="`device-channel-ao-${channel.id}`"
          >
            <input
              ref="aoInputRef"
              :id="`device-channel-ao-${channel.id}`"
              :name="`device-channel-ao-${channel.id}`"
              v-model="aoDraftValue"
              type="number"
              inputmode="decimal"
              min="4"
              max="20"
              step="0.1"
              autocomplete="off"
              class="w-24 min-w-0 flex-1 rounded border border-neutral-300 bg-white px-2 py-1 text-right text-xs font-semibold text-neutral-900 outline-none transition focus:border-sky-500 dark:border-neutral-600 dark:bg-neutral-900 dark:text-neutral-100"
              :disabled="disabled"
              @keydown.enter.prevent="submitAoValue"
            />
            <span class="shrink-0 text-neutral-400 dark:text-neutral-500">mA</span>
          </label>
          <UiButton
            type="button"
            size="xs"
            variant="secondary"
            class="h-5 shrink-0 px-1.5 text-[10px] uppercase tracking-[0.08em]"
            :disabled="!aoCanSubmit"
            :title="aoSetButtonTitle"
            @click.stop="submitAoValue"
          >
            {{ aoSetButtonLabel }}
          </UiButton>

          <div class="inline-flex items-center gap-1 px-0.5 py-0.5">
            <span class="text-[9px] uppercase tracking-[0.08em] text-neutral-400 dark:text-neutral-500">Value</span>
            <span class="font-mono text-[10px] text-neutral-600 dark:text-neutral-300">{{ aoActualValueLabel }}</span>
            <span class="text-[9px] text-neutral-400 dark:text-neutral-500">mA</span>
          </div>

          <span
            v-if="aoStatusClass"
            class="inline-flex items-center rounded px-1.5 py-1 text-[10px] font-medium border"
            :class="aoStatusClass"
            :title="aoStatusTitle"
          >
            {{ aoStatusLabel }}
          </span>
        </div>
      </div>

    </div>
  </div>
</template>


<script setup lang="ts">
import { computed, ref, watch } from "vue"
import UiButton from "@/components/ui/UiButton.vue"
import type { AoChannel, Channel, DiChannel, DoChannel } from "@/types/channel"
import { formatAoValue } from "@/utils/channel"

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

const aoInputRef = ref<HTMLInputElement | null>(null)
const aoDraftValue = ref("")

watch(
  () => [aoChannel.value?.id ?? null, aoChannel.value?.state ?? null] as const,
  ([, state]) => {
    if (typeof state === "number" && Number.isFinite(state)) {
      aoDraftValue.value = formatAoValue(state)
      return
    }
    aoDraftValue.value = ""
  },
  { immediate: true }
)

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
const aoActualValueLabel = computed(() => {
  if (typeof aoChannel.value?.state !== "number" || !Number.isFinite(aoChannel.value.state)) {
    return "—"
  }
  return formatAoValue(aoChannel.value.state)
})

const aoDraftNumber = computed<number | null>(() => {
  const raw = aoDraftValue.value.trim()
  if (!raw) return null
  const parsed = Number.parseFloat(raw)
  if (!Number.isFinite(parsed)) return null
  return parsed
})

const aoDraftChanged = computed(() => {
  if (aoDraftNumber.value == null || aoChannel.value == null) return false
  return formatAoValue(aoDraftNumber.value) !== formatAoValue(aoChannel.value.state)
})

const aoCanSubmit = computed(() => !props.disabled && aoDraftNumber.value !== null)

const aoSetButtonLabel = computed(() => {
  if (aoStatus.value === "pending") return "Wait"
  if (aoHasError.value) return "Retry"
  return "Set"
})

const aoSetButtonTitle = computed(() => {
  if (props.disabled) return "Device is offline"
  if (!aoDraftValue.value.trim()) return "Enter a numeric AO value"
  if (aoDraftNumber.value == null) return "Value must be numeric"
  if (!aoDraftChanged.value) return `Send ${formatAoValue(aoDraftNumber.value)} mA again`
  return `Send ${formatAoValue(aoDraftNumber.value)} mA to device`
})

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

function submitAoValue() {
  const raw = aoInputRef.value?.value?.trim() ?? aoDraftValue.value.trim()
  if (!raw) {
    return
  }
  const exactValue = Number.parseFloat(raw)
  if (!Number.isFinite(exactValue) || props.disabled) {
    return
  }
  aoDraftValue.value = raw
  emit("set-ao", { channel: props.channel, value: exactValue })
}
</script>
