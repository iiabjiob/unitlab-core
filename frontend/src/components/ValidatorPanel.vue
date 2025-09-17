<template>
  <div class="h-full w-full flex flex-col bg-white dark:bg-neutral-900">
    <!-- Header line is provided by wrapper; this is the scrollable body -->
    <div class="flex-1 overflow-y-auto p-3 text-xs text-neutral-700 dark:text-neutral-300">
      <p class="mb-1 font-semibold">Validation Results:</p>
      <ul class="list-disc pl-4 space-y-1">
        <li v-for="(msg, i) in messages" :key="i">{{ msg }}</li>
      </ul>
    </div>
  </div>
</template>

<script setup lang="ts">
defineProps<{
  messages: string[]
}>()
</script>
