<template>
  <div>
    <!-- Table для стандартных полей -->
    <table class="w-full text-xs border-collapse">
      <tbody class="border border-neutral-200 dark:border-neutral-700">
        <tr
          v-for="(field, fieldIdx) in visibleNormalFields"
          :key="getFieldName(field)"
          class="border-b border-neutral-200 dark:border-neutral-700"
        >
          <!-- Label -->
          <td class="px-1 py-0.5 text-neutral-600 dark:text-neutral-400 w-1/3">
            {{ typeof field.label === "function"
              ? field.label(props.item, fieldIdx)
              : field.label }}
          </td>

          <!-- Value -->
          <td class="px-1 py-0.5">
            <!-- Display override -->
            <span v-if="field.display && !field.editable" class="text-neutral-800 dark:text-neutral-200">
              {{ field.display(props.item) }}
            </span>

            <!-- Editable string -->
            <input
              v-else-if="field.type === 'string'"
              type="text"
              :name="getFieldName(field)"
              class="w-full text-xs px-1 py-0.5 bg-transparent focus:outline-none"
              :class="field.editable
                ? ['bg-white', 'dark:bg-neutral-900']
                : ['bg-neutral-100', 'dark:bg-neutral-800']"
              :value="getValue(field)"
              :disabled="!field.editable"
              @change="e => commit(field, (e.target as HTMLInputElement).value)"
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
              :value="getValue(field)"
              :disabled="!field.editable"
              @change="e => commit(field, (e.target as HTMLInputElement).value)"
            />

            <!-- Editable boolean -->
            <input
              v-else-if="field.type === 'boolean'"
              type="checkbox"
              :name="getFieldName(field)"
              class="h-3 w-3"
              :checked="getValue(field)"
              :disabled="!field.editable"
              @change="e => commit(field, (e.target as HTMLInputElement).checked)"
            />

            <!-- Editable select -->
            <UiSelect
              v-else-if="field.type === 'select'"
              :model-value="getValue(field)"
              :name="getFieldName(field)"
              @update:modelValue="val => commit(field, val)"
            >
              <option
                v-for="opt in (field as SelectPropertyField<any>).options"
                :key="opt"
                :value="opt"
              >
                {{ opt }}
              </option>
            </UiSelect>

            <UnitSelect
              v-else-if="field.type === 'unit'"
              :model-value="getValue(field) as string | null"
              :name="field.key"
              @update:modelValue="val => commit(field, val)"
            />

            <BitmaskEditor
              v-else-if="field.type === 'bitmask'"
              :model-value="getValue(field) as number"
              :name="field.key"
              :channel-count="5"
              @update:modelValue="val => commit(field, val)"
            />

            <ChannelSelect
              v-else-if="field.type === 'channel'"
              :model-value="getValue(field) as number | null"
              :channel-type="field.channelType"
              :name="field.key"
              @update:modelValue="(val: number | null) => commit(field, val)"
            />

            <!-- fallback -->
            <span v-else class="italic text-neutral-400">n/a</span>
          </td>
        </tr>
      </tbody>
    </table>

    <template v-for="field in visibleCustomFields" :key="field.key">
      <component
        :is="field.component"
        v-bind="typeof field.props === 'function' ? field.props(props.item) : field.props"
      />
    </template>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import type { CustomPropertyField, PropertyField, PropertySchema, SelectPropertyField } from "@/types/propertySchema"
import ChannelSelect from "./ui/ChannelSelect.vue"
import UnitSelect from "./ui/UnitSelect.vue";
import BitmaskEditor from "./ui/BitmaskEditor.vue";
import UiSelect from "./ui/UiSelect.vue";

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

// Универсальный commit
function commit(field: PropertyField<any>, raw: any) {
  let value: any
  if (field.type === "boolean") {
    value = !!raw
  } else if (field.type === "number") {
    value = raw === "" ? null : Number(raw)
  } else {
    value = raw
  }
  emit("update", field.key, value)
}

// Достаём значение, включая payload.*
function getValue(field: PropertyField<any>) {
  if (field.key.includes(".")) {
    const [root, sub] = field.key.split(".")
    return props.item[root]?.[sub]
  }
  return props.item[field.key]
}

// Только обычные поля, у которых visible = true
const visibleNormalFields = computed(() =>
  props.schema.fields.filter(
    f => f.type !== "custom" && (f.visible?.(props.item) ?? true)
  )
)

const visibleCustomFields = computed(() =>
  props.schema.fields.filter(
    (f): f is CustomPropertyField<any> =>
      f.type === "custom" && (f.visible?.(props.item) ?? true)
  )
)

</script>
