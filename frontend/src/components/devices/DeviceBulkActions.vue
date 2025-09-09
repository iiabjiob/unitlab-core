<template>
  <div
    class="flex flex-wrap justify-start gap-3 items-center px-4 py-2 border border-neutral-200 dark:border-neutral-700 bg-white dark:bg-neutral-800 rounded">
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
    <label class="flex items-center gap-2 text-sm cursor-pointer select-none">
      <input
        type="checkbox"
        v-model="onlyOnline"
        name="online-only-checkbox"
        class="h-4 w-4 rounded border-neutral-400 dark:border-neutral-500
              focus:ring-neutral-600 dark:focus:ring-neutral-400 accent-neutral-600 dark:accent-neutral-400"
      />
      Online only
    </label>

    <!-- Device type filter -->
    <Listbox v-model="selectedTypes" multiple>
      <div class="relative">
        <ListboxButton
          class="border rounded px-3 py-1 text-sm bg-white dark:bg-neutral-700 dark:text-white flex items-center gap-2 border-neutral-200 dark:border-neutral-800 cursor-pointer"
        >
          <span v-if="selectedTypes.length">
            {{ selectedTypes.join(", ") }}
          </span>
          <span v-else class="text-neutral-800 dark:text-neutral-200">All types</span>
          <ChevronDownIcon size="20"/>
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
import { ref, watch, onMounted } from "vue"
import { Listbox, ListboxButton, ListboxOptions, ListboxOption } from "@headlessui/vue"
import ChevronDownIcon from "../icons/ChevronDownIcon.vue"

// --- Emits ---
const emit = defineEmits<{
  (e: "update:filters", filters: { onlyOnline: boolean; types: string[] }): void
  (e: "bulk-delete", ids: string[]): void
  (e: "bulk-activate", ids: string[]): void
}>()

// --- State ---
const selectedIds = ref<string[]>([])
const onlyOnline = ref(false)
const selectedTypes = ref<string[]>([])
const allTypes = ["DI", "DO", "AO"]

// --- Load saved filters on mount ---
onMounted(() => {
  const savedFilters = localStorage.getItem("device-filters")
  if (savedFilters) {
    try {
      const parsed = JSON.parse(savedFilters)
      onlyOnline.value = parsed.onlyOnline ?? false
      selectedTypes.value = parsed.types ?? []
    } catch (err) {
      console.warn("Failed to parse saved filters", err)
    }
  }
})

// --- Watch filters (save + emit) ---
watch([onlyOnline, selectedTypes], () => {
  const filters = {
    onlyOnline: onlyOnline.value,
    types: selectedTypes.value,
  }

  // Save
  localStorage.setItem("device-filters", JSON.stringify(filters))

  // Emit
  emit("update:filters", filters)
}, { deep: true })

// --- Methods ---
function bulkDelete() {
  emit("bulk-delete", selectedIds.value)
  selectedIds.value = []
}

function bulkActivate() {
  emit("bulk-activate", selectedIds.value)
  selectedIds.value = []
}
</script>
