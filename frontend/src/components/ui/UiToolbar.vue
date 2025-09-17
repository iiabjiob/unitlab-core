<template>
  <div
    class="flex items-center justify-between px-3 py-2
           border border-neutral-200 dark:border-neutral-700 rounded-sm shadow"
  >
    <!-- Left section -->
    <div class="flex items-center gap-0.5">
      <h2 v-if="title" class="font-semibold text-sm me-4">
        {{ title }}
      </h2>
      <slot name="left" />
    </div>

    <!-- Center section -->
    <div class="flex-1 flex justify-center">
      <slot name="center" />
    </div>

    <!-- Right section -->
    <div class="flex items-center gap-2">
      <slot name="right" />
    </div>
  </div>
</template>

<script setup lang="ts">
const props = defineProps<{
  title?: string
}>()
</script>
