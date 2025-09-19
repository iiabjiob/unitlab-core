<template>
  <td class="px-1 py-0.5">
    <!-- Display-only -->
    <span
      v-if="field.display && !field.editable"
      class="text-neutral-800 dark:text-neutral-200"
    >
      {{ field.display(item) }}
    </span>

    <!-- Editable -->
    <component
      v-else
      :is="editor"
      v-bind="inputProps"
      class="w-full text-xs px-1 py-0.5 focus:outline-none
        disabled:cursor-not-allowed
        bg-white dark:bg-neutral-900
        disabled:bg-neutral-100 disabled:dark:bg-neutral-800"
      @change="onChange"
      @update:modelValue="onUpdate"
    >
      <!-- Options for select -->
      <option
        v-if="field.type === PROPERTY_FIELD_TYPES.SELECT"
        v-for="opt in (field as any).options"
        :key="opt"
        :value="opt"
      >
        {{ opt }}
      </option>
    </component>
  </td>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { fieldEditors } from "@/property-schemas/fieldEditors";
import { PROPERTY_FIELD_TYPES, type PropertyField } from "@/property-schemas/types";

const props = defineProps<{
  field: PropertyField<any>
  item: Record<string, any>
  value: any
}>()

const emit = defineEmits<{
  (e: "commit", field: PropertyField<any>, value: any): void
}>()

// Select editor component from registry
const editor = computed(() => fieldEditors[props.field.type] ?? "span")

// Generate props for editor
const inputProps = computed(() => {
  const f = props.field
  const common = {
    name: f.key,
    autocomplete: "off"
  }

  switch (f.type) {
    case PROPERTY_FIELD_TYPES.STRING:
      return { ...common, type: "text", value: props.value, disabled: !f.editable }
    case PROPERTY_FIELD_TYPES.NUMBER:
      return { ...common, type: "number", value: props.value, disabled: !f.editable }
    case PROPERTY_FIELD_TYPES.BOOLEAN:
      return { ...common, type: "checkbox", checked: props.value, disabled: !f.editable }
    case PROPERTY_FIELD_TYPES.SELECT:
    case PROPERTY_FIELD_TYPES.UNIT:
    case PROPERTY_FIELD_TYPES.CHANNEL:
      return { ...common, modelValue: props.value, disabled: !f.editable }
    case PROPERTY_FIELD_TYPES.BITMASK:
      return {
        ...common,
        modelValue: props.value,
        channelCount: f.resolveChannelCount(props.item),
        disabled: !f.editable,
      }
    default:
      return {}
  }
})

// Handle native inputs
function onChange(e: Event) {
  const f = props.field
  const target = e.target as HTMLInputElement
  let value: any = target.value

  if (f.type === PROPERTY_FIELD_TYPES.BOOLEAN) value = target.checked
  if (f.type === PROPERTY_FIELD_TYPES.NUMBER) value = target.value === "" ? null : Number(target.value)

  emit("commit", f, value)
}

// Handle v-model based editors
function onUpdate(val: any) {
  emit("commit", props.field, val)
}
</script>
