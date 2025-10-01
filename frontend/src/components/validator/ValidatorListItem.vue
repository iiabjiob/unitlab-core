<template>
  <li
    class="cursor-pointer hover:underline flex items-center gap-2"
    @click="$emit('select', error)"
  >
    <span
      v-if="error.level !== 'warning'"
      class="text-red-600 dark:text-red-400 font-bold"
    >
      🛑
    </span>
    <span
      v-else
      class="text-yellow-600 dark:text-yellow-400 font-bold"
    >
      ⚠️
    </span>
    <span>[{{ error.schemaName }}] – {{ error.message }}</span>
  </li>
</template>

<script setup lang="ts">
import type { SchemaName } from '@/property-schemas/types';
import type { ValidationLevel } from '@/validators/types';

const props = defineProps<{
  error: {
    level: ValidationLevel
    schemaName: SchemaName
    itemId: number | string
    message: string
  }
}>()

defineEmits<{ (e: "select", err: typeof props.error): void }>()
</script>
