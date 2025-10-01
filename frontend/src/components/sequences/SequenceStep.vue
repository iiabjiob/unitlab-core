<template>
  <li class="flex items-center gap-3 group py-1">
    <!-- Drag handle -->
    <slot name="prefix" />

    <!-- Step number -->
    <span class="font-mono text-xs text-neutral-500 w-6 text-right">
      {{ index + 1 }}
    </span>

    <!-- Description -->
    <span class="flex-1 text-sm">
      {{ description }}
    </span>

    <!-- Result icon -->
    <span v-if="completed" class="text-green-600">
      <!-- <SuccessIcon size="16" /> -->
      ✅
    </span>
    <span v-else-if="error" class="text-red-600">
      <!-- <FailIcon size="16" /> -->
      ⚠️
    </span>

    <!-- Delete button (on hover only) -->
    <button
      @click.stop="$emit('delete')"
      class="opacity-0 group-hover:opacity-100 transition-opacity text-neutral-400 hover:text-red-600 cursor-pointer ml-2"
    >
      <TrashIcon size="14" />
    </button>
  </li>
</template>

<script setup lang="ts">
import TrashIcon from "../icons/TrashIcon.vue"

const props = defineProps<{
  index: number
  description: string
  completed: boolean
  error?: string | boolean | null
}>()

const emit = defineEmits<{
  (e: "delete"): void
}>()
</script>
