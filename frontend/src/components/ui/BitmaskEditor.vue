<template>
  <div class="space-y-2">
    <!-- Controls -->
    <div v-if="channelCount>0" class="flex justify-start gap-2 text-xs">
      <UiButton type="toolbar" size="xs" :disabled="isDisabled" @click="selectAll">
        all
      </UiButton>

      <UiButton type="toolbar" size="xs" :disabled="isDisabled" @click="clearAll">
        clear
      </UiButton>

      <UiButton type="toolbar" size="xs" :disabled="isDisabled" @click="invertAll">
        invert
      </UiButton>
    </div>
    <span v-else class="text-xs text-neutral-500">
      unit not found
    </span>

    <!-- Dynamic grid -->
    <div :class="['grid gap-1 text-xs', gridColsClass, isDisabled ? 'opacity-60' : '']">
      <button
        v-for="i in channelCount"
        :key="i"
        type="button"
        class="flex items-center justify-center border rounded-sm h-6 w-6"
        :class="isBitSet(i - 1)
          ? ['bg-neutral-600', 'text-white', 'dark:bg-neutral-300', 'dark:text-black']
          : ['bg-neutral-100', 'dark:bg-neutral-800']"
        :disabled="isDisabled"
        @click="toggleBit(i - 1)"
      >
        {{ i - 1 }}
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import UiButton from "./UiButton.vue";

const props = defineProps<{
  modelValue?: number
  channelCount: number   // теперь приходит от устройства
  disabled?: boolean
}>()

const emit = defineEmits<{
  (e: "update:modelValue", value: number): void
}>()

const value = computed(() => (props.modelValue ?? 0) >>> 0)
const isDisabled = computed(() => Boolean(props.disabled) || props.channelCount <= 0)
const FULL_32_BIT_MASK = 0xffffffff >>> 0

function maskForCount(count: number): number {
  if (count <= 0) return 0
  if (count >= 32) return FULL_32_BIT_MASK
  return (1 << count) - 1
}

// сетка — до 8 колонок, остальное переносится
const gridColsClass = computed(() => {
  if (props.channelCount <= 4) return "grid-cols-4"
  if (props.channelCount <= 8) return "grid-cols-8"
  if (props.channelCount <= 16) return "grid-cols-8"
  return "grid-cols-8" // всегда по 8 колонок
})

function isBitSet(bit: number): boolean {
  return (value.value & (1 << bit)) !== 0
}

function toggleBit(bit: number) {
  if (isDisabled.value) return
  const newValue = (value.value ^ (1 << bit)) >>> 0
  emit("update:modelValue", newValue)
}

function selectAll() {
  if (isDisabled.value) return
  emit("update:modelValue", maskForCount(props.channelCount))
}

function clearAll() {
  if (isDisabled.value) return
  emit("update:modelValue", 0)
}

function invertAll() {
  if (isDisabled.value) return
  const mask = maskForCount(props.channelCount)
  emit("update:modelValue", ((~value.value) & mask) >>> 0)
}
</script>
