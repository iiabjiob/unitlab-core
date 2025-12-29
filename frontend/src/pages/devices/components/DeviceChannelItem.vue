<template>
  <div
    class="group flex items-center justify-between px-2 py-1.5 text-sm select-none"
  >
    <div class="flex items-center gap-3">

      <!-- DO control -->
      <div
        v-if="channel.type === 'do'"
        class="w-4 h-4 rounded-sm border cursor-pointer flex items-center justify-center
               transition-colors"
        :class="[doControlClass, { 'cursor-not-allowed opacity-70': isWaiting }]"
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
        v-else-if="channel.type === 'di'"
        class="w-4 h-4 rounded-full border"
        :class="{
          'bg-green-500 border-green-600': channel.state,
          'bg-neutral-600 border-neutral-500': !channel.state
        }"
      />

      <!-- AO input -->
      <input
        v-else-if="channel.type === 'ao'"
        type="number"
        class="w-16 px-1 py-0.5 text-xs rounded border border-neutral-600
               bg-neutral-900 text-neutral-200"
        :value="channel.state"
        @change="onAoChange"
      />

      <!-- Label -->
      <div class="text-xs text-neutral-700 dark:text-neutral-300">
        {{ channel.resolved_name }}
      </div>
    </div>
  </div>
</template>


<script setup lang="ts">
import { computed } from "vue"
import type { Channel } from "@/types/channel"

const props = defineProps<{ channel: Channel }>()

const emit = defineEmits<{
  (e: "toggle", ch: Channel): void
  (e: "set-ao", payload: { channel: Channel; value: number }): void
}>()

const status = computed(() =>
  props.channel.type === "do" ? props.channel.ui?.stage ?? "idle" : "idle"
)
const isWaiting = computed(() => status.value === "pending" || status.value === "debounce")
const isError = computed(() => status.value === "error")

const doControlClass = computed(() => {
  if (props.channel.type !== "do") {
    return ""
  }
  if (isError.value) {
    return "bg-red-500 border-red-500 text-white animate-pulse"
  }
  if (props.channel.state) {
    return "bg-green-500 border-green-600 text-white"
  }
  if (isWaiting.value) {
    return "bg-yellow-400/80 border-yellow-500 text-yellow-900"
  }
  return "bg-neutral-300 dark:bg-neutral-700 border-neutral-600 hover:bg-neutral-600"
})

function onToggleClick() {
  if (props.channel.type !== "do" || isWaiting.value) {
    return
  }
  emit("toggle", props.channel)
}

function onAoChange(event: Event) {
  const raw = (event.target as HTMLInputElement).value
  const num = Number(raw)
  if (!isNaN(num)) {
    emit("set-ao", { channel: props.channel, value: num })
  }
}
</script>
