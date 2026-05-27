<template>
  <div class="bitmask-editor">
    <div v-if="channelCount > 0" class="bitmask-editor__controls">
      <UiButton type="button" variant="toolbar" size="xs" :disabled="isDisabled" @click="selectAll">
        all
      </UiButton>

      <UiButton type="button" variant="toolbar" size="xs" :disabled="isDisabled" @click="clearAll">
        clear
      </UiButton>

      <UiButton type="button" variant="toolbar" size="xs" :disabled="isDisabled" @click="invertAll">
        invert
      </UiButton>
    </div>
    <span v-else class="bitmask-editor__empty">
      unit not found
    </span>

    <div
      class="bitmask-editor__grid"
      :class="{ 'is-disabled': isDisabled }"
      :style="gridStyle"
    >
      <button
        v-for="i in channelCount"
        :key="i"
        type="button"
        class="bitmask-editor__bit"
        :class="{ 'is-set': isBitSet(i - 1) }"
        :disabled="isDisabled"
        @click="toggleBit(i - 1)"
      >
        {{ i }}
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

const gridColumnCount = computed(() => props.channelCount <= 4 ? 4 : 8)
const gridStyle = computed(() => ({
  gridTemplateColumns: `repeat(${gridColumnCount.value}, minmax(0, 1.5rem))`,
}))

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

<style scoped>
.bitmask-editor {
  display: grid;
  gap: 0.5rem;
}

.bitmask-editor__controls {
  display: flex;
  font-size: var(--text-xs);
  gap: 0.5rem;
  justify-content: flex-start;
  line-height: 1rem;
}

.bitmask-editor__empty {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  line-height: 1rem;
}

.bitmask-editor__grid {
  display: grid;
  font-size: var(--text-xs);
  gap: 0.25rem;
  line-height: 1rem;
}

.bitmask-editor__grid.is-disabled {
  opacity: 0.6;
}

.bitmask-editor__bit {
  align-items: center;
  background: var(--color-neutral-100);
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-sm);
  display: flex;
  height: 1.5rem;
  justify-content: center;
  width: 1.5rem;
}

.bitmask-editor__bit.is-set {
  background: var(--color-neutral-600);
  color: var(--color-white);
}

.dark .bitmask-editor__bit {
  background: var(--color-neutral-800);
  border-color: var(--color-neutral-700);
}

.dark .bitmask-editor__bit.is-set {
  background: var(--color-neutral-300);
  color: var(--color-neutral-950);
}
</style>
