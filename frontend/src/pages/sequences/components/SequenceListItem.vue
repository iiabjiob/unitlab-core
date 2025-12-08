<script setup lang="ts">
import type { SequenceDef } from "@/types/sequences"

defineProps<{
  sequence: SequenceDef
  active: boolean
}>()
</script>

<template>
  <div
    class="group cursor-pointer select-none flex items-center px-3 py-2
           text-neutral-400 dark:text-neutral-600 hover:bg-neutral-850 transition-colors"
    :class="{
      'text-neutral-900 dark:text-white': active
    }"
  >
    <!-- Active indicator -->
    <div
      class="w-1 h-5 mr-2 rounded transition-colors"
      :class="active ? 'bg-blue-500' : 'bg-transparent group-hover:bg-neutral-700'"
    />

    <!-- Title -->
    <div class="truncate text-sm">
      {{ sequence.name }}
    </div>
  </div>
</template>