<template>
  <tr
    class="border-b border-neutral-200 dark:border-neutral-700"
    :data-schema="schemaName"
    :data-item="item.id"
    :data-field="field.key"
    >
    <td
      class="px-1 py-0.5 text-neutral-600 dark:text-neutral-400 w-1/3 label-cell"
      :class="{ 'bg-red-50 dark:bg-red-900/30': hasError }"
      >
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
import { useValidationStore } from "@/stores/validationStore";
import { computed } from "vue";

const props = defineProps<{
  field: PropertyField<any>
  item: Record<string, any>
  index: number
  value: any
  schemaName?: string
  itemId?: string | number
}>()

const emit = defineEmits<{
  (e: "commit", field: PropertyField<any>, value: any): void
}>()

const validationStore = useValidationStore()

// Список ошибок именно для этого поля
const fieldErrors = computed(() =>
  props.schemaName && props.itemId
    ? validationStore.getErrors(props.schemaName, props.itemId, props.field.key)
    : []
)

// Флаг для подсветки
const hasError = computed(() => fieldErrors.value.length > 0)

</script>
