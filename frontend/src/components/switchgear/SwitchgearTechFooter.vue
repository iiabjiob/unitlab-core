<!-- src/components/switchgear/SwitchgearTechFooter.vue -->
<template>
  <div class="text-xs text-neutral-500 dark:text-neutral-400">
    <div>DO: {{ doUnitId }} [{{ doOpenCh }}|{{ doCloseCh }}]</div>
    <div>DI: {{ diUnitId }} [{{ diOpenPulseCh }}|{{ diClosePulseCh }}]</div>
    <div class="flex items-center gap-2 mt-1">
      <span>Feedback delay:</span>
      <input
        name="feedbackDelayMs"
        type="number"
        min="0"
        step="100"
        v-model.number="model"
        class="w-16 px-1 py-0.5 text-xs rounded border border-neutral-300 dark:border-neutral-600 bg-white dark:bg-neutral-700 text-neutral-700 dark:text-neutral-200"
      />
      <span>ms</span>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'

const props = defineProps<{
  doUnitId: string
  doOpenCh: number
  doCloseCh: number
  diUnitId: string
  diOpenPulseCh: number
  diClosePulseCh: number
  modelValue: number
}>()
const emit = defineEmits<{ (e: "update:modelValue", v: number): void }>()
const model = computed({
  // Read from prop
  get: () => props.modelValue,
  // Emit update on write
  set: (v: number) => emit("update:modelValue", v),
})
</script>
