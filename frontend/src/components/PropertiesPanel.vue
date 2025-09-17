<template>
  <div>
    <!-- Table для стандартных полей -->
    <table class="w-full text-xs border-collapse">
      <tbody class="border border-neutral-200 dark:border-neutral-700">
        <tr
          v-for="(field, fieldIdx) in normalFields"
          :key="field.key"
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
              :value="props.item[field.key] ?? ''"
              :disabled="!field.editable"
              @change="e => commit(field, (e.target as HTMLInputElement).value)"
            />

            <!-- Editable boolean -->
            <input
              v-else-if="field.type === 'boolean'"
              type="checkbox"
              :name="getFieldName(field)"
              class="h-3 w-3"
              :checked="props.item[field.key] ?? false"
              :disabled="!field.editable"
              @change="e => commit(field, (e.target as HTMLInputElement).checked)"
            />

            <!-- Channel select -->
            <ChannelSelect
              v-else-if="field.type === 'channel'"
              :model-value="props.item[field.key] as number | null"
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

    <!-- Custom поля -->
    <template v-for="field in customFields" :key="field.key">
      <component
        :is="field.component"
        v-bind="typeof field.props === 'function' ? field.props(item) : field.props"
      />
    </template>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import type { PropertyField, PropertySchema } from "@/types/propertySchema"
import ChannelSelect from "./ui/ChannelSelect.vue"

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

// универсальный commit для всех типов полей
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

const normalFields = computed(() =>
  props.schema.fields.filter(f => f.type !== "custom")
)

const customFields = computed(() =>
  props.schema.fields.filter(f => f.type === "custom")
)
</script>
