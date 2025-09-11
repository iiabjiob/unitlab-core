<template>
  <div
    class="h-2 rounded bg-neutral-200 dark:bg-neutral-700 overflow-hidden"
    :class="wrapperClass"
  >
    <div
      class="h-full transition-all duration-300"
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
  barClass: "bg-neutral-600",
})

const clampedValue = computed(() => Math.min(100, Math.max(0, props.value)))
</script>
