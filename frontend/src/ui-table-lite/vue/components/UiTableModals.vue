<template>
  <AdvancedFilterModal
    :open="advancedOpen"
    :column-label="advancedColumnLabel"
    :type="advancedType"
    :condition="advancedCondition"
    @apply="value => emit('advanced-apply', value)"
    @clear="() => emit('advanced-clear')"
    @cancel="() => emit('advanced-cancel')"
  />
</template>

<script setup lang="ts">
import { toRefs } from "vue"
import AdvancedFilterModal from "./AdvancedFilterModal.vue"
import type { FilterCondition } from "../composables/useTableFilters"

const props = defineProps<{
  advancedOpen: boolean
  advancedColumnLabel: string
  advancedType: FilterCondition["type"]
  advancedCondition: FilterCondition | null
}>()

const {
  advancedOpen,
  advancedColumnLabel,
  advancedType,
  advancedCondition,
} = toRefs(props)

const emit = defineEmits<{
  (event: "advanced-apply", payload: FilterCondition | null): void
  (event: "advanced-clear"): void
  (event: "advanced-cancel"): void
}>()
</script>
