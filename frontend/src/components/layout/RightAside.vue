<template>
  <div class="h-full w-full flex flex-col bg-neutral-100 dark:bg-neutral-800">
    <div class="flex items-center justify-end px-3 pt-3 font-semibold">
      <button
        class="w-6 h-6 flex items-center justify-center rounded hover:bg-neutral-200 dark:hover:bg-neutral-700"
        @click="$emit('collapse')"
      >
        ✖
      </button>
    </div>

    <GenericPropertyPanel
      v-if="selection.selected"
      :schema="resolveSchema(selection.selected.type)"
      :item="selection.selected.item"
      @update="onUpdate"

    />
    <div v-else class="flex-1 flex items-center justify-center text-xs text-neutral-500">
      No item selected
    </div>
  </div>
</template>

<script setup lang="ts">
defineEmits<{ (e: "collapse"): void }>()

import { resolveSchema } from "@/property-schemas/propertySchemas"
import { useSelectionStore } from "@/stores/selectionStore";
import GenericPropertyPanel from "../GenericPropertyPanel.vue";

const selection = useSelectionStore()

async function onUpdate<T>(key: keyof T, value: any) {
  if (!selection.selected) return
  const schema = resolveSchema(selection.selected.type)
  try {
    await schema.update(selection.selected.item, key as any, value)
  } catch (err) {
    console.error("Update failed", err)
  }
}

</script>
