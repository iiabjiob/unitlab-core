<template>
  <div
    class="flex items-center justify-between px-3 py-1 text-xs border-t border-neutral-300 dark:border-neutral-700 bg-neutral-200 dark:bg-neutral-700 cursor-pointer"
    @click="$emit('toggle')"
  >
    <div class="flex items-center gap-2">
      <h4 class="font-bold text-sm">Validator</h4>
      <span class="text-red-700 dark:text-red-300">🛑 {{ errorsCount }}</span>
      <span class="text-yellow-700 dark:text-yellow-300">⚠️ {{ warningsCount }}</span>
    </div>
    <span class="text-neutral-600 dark:text-neutral-300">
      {{ showValidator ? "Hide" : "Show" }}
    </span>
  </div>
</template>

<script setup lang="ts">
defineProps<{
  errorsCount: number
  warningsCount: number
  showValidator: boolean
}>()

defineEmits<{
  (e: "toggle"): void
}>()
</script>
