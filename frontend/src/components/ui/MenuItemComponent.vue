<template>
  <MenuItem v-slot="{ active }">
    <button
      @click.stop="$emit('click')"
      class="block w-full px-4 py-2 text-sm text-left"
      :class="[
        active ? 'bg-neutral-100 dark:bg-neutral-600' : '',
        danger
          ? 'text-red-600 dark:text-red-400'
          : 'text-neutral-700 dark:text-neutral-200'
      ]"
    >
      <slot />
    </button>
  </MenuItem>
</template>

<script setup lang="ts">
import { MenuItem } from "@headlessui/vue"

defineProps<{ danger?: boolean }>()
defineEmits<{ (e: "click"): void }>()
</script>
