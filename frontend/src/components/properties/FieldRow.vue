<template>
  <tr class="border-b border-neutral-200 dark:border-neutral-700">
    <td class="px-1 py-0.5 text-neutral-600 dark:text-neutral-400 w-1/3">
      {{ typeof field.label === "function" ? field.label(item, index) : field.label }}
    </td>
    <FieldRenderer
      :field="field"
      :item="item"
      :value="value"
      @commit="(field, value) => emit('commit', field, value)"
    />
  </tr>
</template>

<script setup lang="ts">
import type { PropertyField } from "@/property-schemas/types";
import FieldRenderer from "./FieldRenderer.vue";

const props = defineProps<{
  field: PropertyField<any>
  item: Record<string, any>
  index: number
  value: any
}>()

const emit = defineEmits<{
  (e: "commit", field: PropertyField<any>, value: any): void
}>()
</script>
