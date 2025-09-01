<script setup lang="ts">
const { status } = defineProps<{
  status: 'online' | 'offline' | string
}>()
</script>

<template>
  <span class="inline-flex items-center gap-1">
    <span class="text-[6px]">
      {{ status === 'online' ? '🟢' : '⚪️' }}
    </span>
    <span
      class="text-sm"
      :class="status === 'online'
        ? 'text-green-500'
        : 'text-gray-500'"
    >
      {{ status }}
    </span>
  </span>
</template>
