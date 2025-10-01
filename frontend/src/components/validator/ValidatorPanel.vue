<template>
  <!-- Заголовок всегда виден -->
  <ValidatorToggle
    :errors-count="errorsCount"
    :warnings-count="warningsCount"
    :show-validator="validation.showValidator"
    @toggle="validation.toggleValidator"
  />

  <!-- Панель появляется только если открыта -->
  <ResizablePanel
    v-if="validation.showValidator"
    class="ignore-selection bg-neutral-100 dark:bg-neutral-800 border-t border-neutral-300 dark:border-neutral-700"
    placement="bottom"
    storageKey="validator-height"
    :defaultSize="validation.panelHeight"
    :minSize="100"
    :maxSize="500"
    @resize-end="validation.setPanelHeight"
  >
    <div class="flex-1 overflow-y-auto p-3 pt-0 text-xs">
      <!-- Filter -->
      <div v-show="errorsCount>0 || warningsCount>0"
        class="flex items-center justify-between py-1 border-b border-neutral-200 dark:border-neutral-700 bg-neutral-100 dark:bg-neutral-800"
      >
        <label class="flex items-center gap-1 text-xs text-neutral-600 dark:text-neutral-300">
          <input name="errorsOnly" type="checkbox" v-model="validation.showErrorsOnly" class="accent-red-500" />
          Errors only
        </label>
      </div>

      <!-- List -->
      <div class="text-neutral-700 dark:text-neutral-300 pt-3">
        <ul class="list-disc space-y-1">
          <ValidatorListItem
            v-for="err in filteredErrors"
            :key="err.schemaName + '-' + err.itemId"
            :error="err"
            @select="$emit('focus-field', err)"
          />
        </ul>
        <div v-if="!filteredErrors.length" class="text-neutral-500">
          No validation errors
        </div>
      </div>
    </div>
  </ResizablePanel>
</template>


<script setup lang="ts">
import { computed } from "vue"
import { useValidationStore } from "@/stores/validationStore"
import ValidatorToggle from "./ValidatorToggle.vue"
import ResizablePanel from "../ui/ResizablePanel.vue"
import ValidatorListItem from "./ValidatorListItem.vue"

const validation = useValidationStore()

defineEmits<{ (e: "focus-field", err: any): void }>()

const filteredErrors = computed(() =>
  validation.showErrorsOnly
    ? validation.errors.filter(e => e.level !== "warning")
    : validation.errors
)

const errorsCount = computed(() =>
  validation.errors.filter(e => e.level !== "warning").length
)
const warningsCount = computed(() =>
  validation.errors.filter(e => e.level === "warning").length
)

</script>
