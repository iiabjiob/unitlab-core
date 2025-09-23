<template>
  <div>
    <!-- Normal fields -->
    <table class="w-full text-xs border-collapse">
      <tbody class="border border-neutral-200 dark:border-neutral-700">
        <FieldRow
          v-for="(field, idx) in visibleNormalFields"
          :key="field.key"
          :field="field"
          :item="item"
          :index="idx"
          :value="getValue(field)"
          :schema-name="schemaName"
          :item-id="item.id"
          @commit="commit"
        />
      </tbody>
    </table>

    <!-- Custom fields -->
    <template v-for="field in visibleCustomFields" :key="field.key">
      <component
        :is="field.component"
        v-bind="typeof field.props === 'function' ? field.props(item) : field.props"
      />
    </template>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import FieldRow from "./FieldRow.vue"
import type { CustomPropertyField, PropertyField, PropertySchema, SchemaName } from "@/property-schemas/types"
import { getValueByPath } from "@/utils/object";

const props = defineProps<{
  schema: PropertySchema<any>
  item: Record<string, any>
  schemaName?: SchemaName
}>()

const emit = defineEmits<{
  (e: "update", key: string, value: any): void
}>()

function commit(field: PropertyField<any>, value: any) {
  emit("update", field.key, value)
}

function getValue(field: PropertyField<any>) {
  return getValueByPath(props.item, field.key)
}

const visibleNormalFields = computed(() =>
  props.schema.fields.filter(f => f.type !== "custom" && (f.visible?.(props.item) ?? true))
)

const visibleCustomFields = computed(() =>
  props.schema.fields.filter(
    (f): f is CustomPropertyField<any> => f.type === "custom" && (f.visible?.(props.item) ?? true)
  )
)
</script>
