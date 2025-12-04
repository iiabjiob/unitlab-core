<template>
  <div
    v-if="useInlineControls"
    class="ui-table__toolbar flex items-center justify-end gap-2 mb-2 relative"
  >
    <button
      type="button"
      class="btn btn-secondary btn-xs flex items-center gap-1 transition-transform transform duration-150 disabled:cursor-not-allowed disabled:opacity-40"
      :class="{ 'hover:-translate-y-0.5 focus:-translate-y-0.5 hover:shadow-sm': hasActiveFiltersOrGroups }"
      :disabled="!hasActiveFiltersOrGroups"
      :name="resetFiltersButtonName"
      @click="$emit('reset-filters')"
    >
      <span>Reset filters</span>
      <span
        v-if="resetTargetsCount"
        class="rounded-full bg-blue-50 px-1 py-0.5 text-[10px] font-semibold text-blue-600 dark:bg-blue-500/20 dark:text-blue-300 transition-colors duration-150"
      >
        {{ resetTargetsCount }}
      </span>
    </button>
    <button
      type="button"
      class="btn btn-secondary btn-xs"
      data-testid="column-visibility-toggle"
      :aria-expanded="showVisibilityPanel"
      aria-haspopup="true"
      aria-label="Toggle column visibility panel"
      :name="columnToggleButtonName"
      @click="$emit('toggle-visibility')"
    >
      Columns
    </button>

    <UiTableColumnVisibility
      v-if="showVisibilityPanel"
      :columns="columns"
      :storage-key="visibilityStorageKey"
      @update="$emit('visibility-update', $event)"
      @close="$emit('visibility-close')"
      @reset="$emit('visibility-reset')"
    />
  </div>
</template>

<script setup lang="ts">
import UiTableColumnVisibility from "./UiTableColumnVisibility.vue"
import type { UiTableColumn } from "../../core/types"

defineProps<{
  useInlineControls: boolean
  hasActiveFiltersOrGroups: boolean
  resetFiltersButtonName: string
  resetTargetsCount: number
  showVisibilityPanel: boolean
  columnToggleButtonName: string
  visibilityStorageKey: string
  columns: UiTableColumn[]
}>()

defineEmits<{
  (event: "reset-filters"): void
  (event: "toggle-visibility"): void
  (event: "visibility-update", payload: { key: string; visible: boolean; label?: string }[]): void
  (event: "visibility-close"): void
  (event: "visibility-reset"): void
}>()
</script>
