<template>
  <div class="ui-table__status-bar">
    <div class="ui-table__status-bar-left">
      <span class="ui-table__status-label">Rows:</span>
      <span class="ui-table__status-value">{{ rowCountDisplay }}</span>
      <template v-if="showSelectedRows">
        <span class="ui-table__status-divider">|</span>
        <span class="ui-table__status-label">Selected rows:</span>
        <span class="ui-table__status-value">{{ selectedRowCountDisplay }}</span>
      </template>
    </div>
    <div class="ui-table__status-bar-right">
      <span class="ui-table__status-label">Selected cells:</span>
      <span class="ui-table__status-value">{{ selectedCellCountDisplay }}</span>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"

const props = defineProps<{
  rowCountDisplay: string
  selectedRowCount: number
  selectedRowCountDisplay: string
  selectedCellCount: number
  selectedCellCountDisplay: string
}>()

const showSelectedRows = computed(() => props.selectedRowCount > 0)
</script>
