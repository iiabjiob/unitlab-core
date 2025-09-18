<template>
  <div class="space-y-2">
    <!-- Controls -->
    <div class="flex justify-start gap-2 text-xs">
      <button
        type="button"
        class="px-2 py-0.5 border rounded-sm
               bg-neutral-100 dark:bg-neutral-800
               hover:bg-neutral-200 dark:hover:bg-neutral-700"
        @click="selectAll"
      >
        all
      </button>
      <button
        type="button"
        class="px-2 py-0.5 border rounded-sm
               bg-neutral-100 dark:bg-neutral-800
               hover:bg-neutral-200 dark:hover:bg-neutral-700"
        @click="clearAll"
      >
        clear
      </button>
      <button
        type="button"
        class="px-2 py-0.5 border rounded-sm
               bg-neutral-100 dark:bg-neutral-800
               hover:bg-neutral-200 dark:hover:bg-neutral-700"
        @click="invertAll"
      >
        invert
      </button>
    </div>

    <!-- Dynamic grid -->
    <div :class="`grid gap-1 text-xs ${gridColsClass}`">
      <button
        v-for="i in channelCount"
        :key="i"
        type="button"
        class="flex items-center justify-center border rounded-sm h-6 w-6"
        :class="isBitSet(i - 1)
          ? ['bg-neutral-600', 'text-white', 'dark:bg-neutral-300', 'dark:text-black']
          : ['bg-neutral-100', 'dark:bg-neutral-800']"
        @click="toggleBit(i - 1)"
      >
        {{ i - 1 }}
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"

const props = defineProps<{
  modelValue?: number
  channelCount: number   // теперь приходит от устройства
}>()

const emit = defineEmits<{
  (e: "update:modelValue", value: number): void
}>()

const value = computed(() => props.modelValue ?? 0)

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
  const newValue = value.value ^ (1 << bit)
  emit("update:modelValue", newValue)
}

function selectAll() {
  const mask = (1 << props.channelCount) - 1
  emit("update:modelValue", mask >>> 0)
}

function clearAll() {
  emit("update:modelValue", 0)
}

function invertAll() {
  const mask = (1 << props.channelCount) - 1
  emit("update:modelValue", (~value.value) & mask)
}
</script>
