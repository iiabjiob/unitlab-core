<template>
  <div
    class="flex flex-wrap gap-3 items-center px-4 py-2 border border-neutral-200 dark:border-neutral-700 bg-white dark:bg-neutral-800 rounded">
    <!-- Bulk actions -->
    <div v-if="selectedIds.length" class="flex gap-2">
      <button
        class="px-3 py-1 rounded bg-red-600 text-white text-sm"
        @click="bulkDelete"
      >
        Delete ({{ selectedIds.length }})
      </button>
      <button
        class="px-3 py-1 rounded bg-green-600 text-white text-sm"
        @click="bulkActivate"
      >
        Activate
      </button>
    </div>

    <!-- Online only toggle -->
    <button
      type="button"
      class="flex items-center gap-2 text-sm px-3 py-1 rounded border border-neutral-300 dark:border-neutral-600 hover:bg-neutral-100 dark:hover:bg-neutral-700"
      :class="onlyOnline ? 'bg-green-100 dark:bg-green-900 text-green-700 dark:text-green-300' : ''"
      @click="toggleOnline"
    >
      <span
        class="inline-block w-4 h-4 border rounded-sm"
        :class="onlyOnline ? 'bg-green-600 border-green-600' : 'border-neutral-400 dark:border-neutral-500'"
      ></span>
      Online only
    </button>

    <!-- Device type filter -->
    <Listbox v-model="selectedTypes" multiple>
      <div class="relative">
        <ListboxButton
          class="border rounded px-3 py-1 text-sm bg-white dark:bg-neutral-700 dark:text-white flex items-center gap-2"
        >
          <span v-if="selectedTypes.length">
            {{ selectedTypes.join(", ") }}
          </span>
          <span v-else class="text-neutral-400">All types</span>
        </ListboxButton>

        <ListboxOptions
          class="absolute mt-1 max-h-60 w-40 overflow-auto rounded-md bg-white dark:bg-neutral-800 shadow-lg border border-neutral-200 dark:border-neutral-600 focus:outline-none z-10"
        >
          <ListboxOption
            v-for="type in allTypes"
            :key="type"
            :value="type"
            class="cursor-pointer select-none px-3 py-1 text-sm hover:bg-neutral-100 dark:hover:bg-neutral-700"
            :class="selectedTypes.includes(type) ? 'bg-neutral-200 dark:bg-neutral-600' : ''"
          >
            {{ type }}
          </ListboxOption>
        </ListboxOptions>
      </div>
    </Listbox>
  </div>
</template>

<script setup lang="ts">
import { ref, watch } from "vue"
import { Listbox, ListboxButton, ListboxOptions, ListboxOption } from "@headlessui/vue"

// --- Emits ---
const emit = defineEmits<{
  (e: "update:filters", filters: { onlyOnline: boolean; types: string[] }): void
  (e: "bulk-delete", ids: string[]): void
  (e: "bulk-activate", ids: string[]): void
}>()

// --- State ---
const selectedIds = ref<string[]>([]) // сюда будут попадать отмеченные карточки устройств
const onlyOnline = ref(false)
const selectedTypes = ref<string[]>([])
const allTypes = ["DI", "DO", "AO"]

// --- Methods ---
function toggleOnline() {
  onlyOnline.value = !onlyOnline.value
}

function bulkDelete() {
  emit("bulk-delete", selectedIds.value)
  selectedIds.value = []
}

function bulkActivate() {
  emit("bulk-activate", selectedIds.value)
  selectedIds.value = []
}

// --- Watch filters ---
watch([onlyOnline, selectedTypes], () => {
  emit("update:filters", {
    onlyOnline: onlyOnline.value,
    types: selectedTypes.value,
  })
})
</script>
