<template>
  <div
    class="progress-bar"
    :class="wrapperClass"
  >
    <div
      class="progress-bar__bar"
      :class="barClass"
      :style="{ width: clampedValue + '%' }"
    ></div>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"

const props = withDefaults(defineProps<{
  value: number          // progress in %
  wrapperClass?: string  // extra styles for outer wrapper
  barClass?: string      // extra styles for inner bar
}>(), {
  wrapperClass: "",
  barClass: "progress-bar__bar--default",
})

const clampedValue = computed(() => Math.min(100, Math.max(0, props.value)))
</script>

<style scoped>
.progress-bar {
  background: var(--color-neutral-200);
  border-radius: 0.25rem;
  height: 0.5rem;
  overflow: hidden;
}

.progress-bar__bar {
  height: 100%;
  transition: width 300ms ease;
}

.progress-bar__bar--default {
  background: var(--color-blue-500);
}

.dark .progress-bar {
  background: var(--color-neutral-700);
}
</style>
