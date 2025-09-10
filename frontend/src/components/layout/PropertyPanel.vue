<template>
  <div class="p-2">
    <h3 class="font-bold mb-2">Properties</h3>
    <table class="w-full text-xs border-collapse">
      <tbody class="border border-neutral-200 dark:border-neutral-700">
        <tr
          v-for="field in props.schema.fields"
          :key="field.key"
          class="border-b border-neutral-200 dark:border-neutral-700"
        >
          <!-- Label -->
          <td class="px-1 py-0.5 text-neutral-600 dark:text-neutral-400 w-1/3">
            {{ field.label }}
          </td>

          <!-- Value -->
          <td class="px-1 py-0.5">
            <!-- Editable string -->
            <input
              v-if="field.type === 'string'"
              type="text"
              :name="getFieldName(field)"
              class="w-full text-xs px-1 py-0.5 bg-transparent focus:outline-none"
              :class="field.editable
                ? ['bg-white', 'dark:bg-neutral-900']
                : ['bg-neutral-100', 'dark:bg-neutral-800']"
              :value="props.item[field.key] ?? ''"
              :disabled="!field.editable"
              @change="commitOnChange(field, $event)"
            />

            <!-- Editable number -->
            <input
              v-else-if="field.type === 'number'"
              type="number"
              :name="getFieldName(field)"
              class="w-full text-xs px-1 py-0.5 bg-transparent focus:outline-none"
              :class="field.editable
                ? ['bg-white', 'dark:bg-neutral-900']
                : ['bg-neutral-100', 'dark:bg-neutral-800']"
              :value="props.item[field.key] ?? ''"
              :disabled="!field.editable"
              @change="commitOnChange(field, $event)"
            />

            <!-- Editable boolean -->
            <input
              v-else-if="field.type === 'boolean'"
              type="checkbox"
              :name="getFieldName(field)"
              class="h-3 w-3"
              :checked="props.item[field.key] ?? false"
              :disabled="!field.editable"
              @change="commitOnChange(field, $event)"
            />

            <!-- <button
              v-else-if="field.type === 'signal'"
              type="button"
              class="w-full text-left text-xs px-1 py-0.5 border rounded bg-white dark:bg-neutral-900"
              @click="openSignalPicker(field)"
              >
              {{ displaySignal(field) }}
            </button> -->

            <!-- <SignalPicker
              v-if="field.type === 'signal'"
              :kind="field.signalKind!"
              :model-unit-id="item[field.unitKey!]"
              :model-channel="item[field.channelKey!]"
              @update="v => {
                emit('update', field.unitKey!, v.unitId)
                emit('update', field.channelKey!, v.channel)
              }"
            /> -->

            <!-- Fallback -->
            <span v-else class="italic text-neutral-400">n/a</span>
          </td>
        </tr>
      </tbody>
    </table>
    <!-- <SignalPickerPanel
      v-if="pickerField"
      :kind="pickerField.signalKind!"
      @pick="onSignalPicked"
      @close="closeSignalPicker"
    /> -->
  </div>
</template>

<script setup lang="ts">
import { ref } from "vue"
import type { PropertyField, PropertySchema } from "@/types/propertySchema"
// import SignalPickerPanel from "../SignalPickerPanel.vue";
import SignalPicker from "../SignalPicker.vue";


const props = defineProps<{
  schema: PropertySchema<any>
  item: Record<string, any>
  itemId?: string | number
}>()

const emit = defineEmits<{
  (e: "update", key: string, value: any): void
}>()

function getFieldName(field: PropertyField<any>) {
  return `prop-${props.itemId ?? "item"}-${field.key}`
}

function commitOnChange(field: PropertyField<any>, e: Event) {
  const el = e.currentTarget as HTMLInputElement

  let value: any
  if (field.type === "boolean") {
    value = el.checked
  } else if (field.type === "number") {
    value = el.value === "" ? null : Number(el.value)
  } else {
    value = el.value
  }

  emit("update", field.key, value)
}

// const pickerField = ref<PropertyField<any> | null>(null)

// function openSignalPicker(field: PropertyField<any>) {
//   pickerField.value = field
// }

// function closeSignalPicker() {
//   pickerField.value = null
// }

// function onSignalPicked(value: { unitId: string; channel: number }) {
//   if (!pickerField.value) return
//   emit("update", pickerField.value.unitKey!, value.unitId)
//   emit("update", pickerField.value.channelKey!, value.channel)
//   pickerField.value = null
// }

// function displaySignal(field: PropertyField<any>) {
//   const unitId = props.item[field.unitKey!]
//   const ch = props.item[field.channelKey!]
//   return unitId ? `${unitId} / Ch ${ch}` : "Select signal…"
// }
</script>
