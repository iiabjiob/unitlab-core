<template>
  <div class="h-full w-full flex flex-col">
    <!-- Filter -->
    <div class="flex items-center justify-between px-3 py-1 border-b border-neutral-200 dark:border-neutral-700 bg-neutral-100 dark:bg-neutral-800">
      <label class="flex items-center gap-1 text-xs text-neutral-600 dark:text-neutral-300">
        <input name="errorsOnly" type="checkbox" v-model="validation.showErrorsOnly" class="accent-red-500" />
        Errors only
      </label>
    </div>

    <!-- List -->
    <div class="flex-1 overflow-y-auto p-3 text-xs text-neutral-700 dark:text-neutral-300">
      <ul class="list-disc space-y-1">
        <li
          v-for="(err, i) in filteredErrors"
          :key="i"
          class="cursor-pointer hover:underline flex items-center gap-2"
          @click="$emit('focus-field', err)"
        >
          <span
            v-if="err.level !== 'warning'"
            class="text-red-600 dark:text-red-400 font-bold"
            title="Error"
          >
            🛑
          </span>
          <span
            v-else
            class="text-yellow-600 dark:text-yellow-400 font-bold"
            title="Warning"
          >
            ⚠️
          </span>
          <span>[{{ err.schemaName }} #{{ err.itemId }}] – {{ err.message }}</span>
        </li>
      </ul>
      <div v-if="!filteredErrors.length" class="text-neutral-500">
        No validation errors
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, watch } from "vue"
import { runGlobalValidation } from "@/property-schemas/runValidation"
import type { ValidationError } from "@/property-schemas/validation"
import { useDeviceStore } from "@/stores/deviceStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useValidationStore } from "@/stores/validationStore"

const deviceStore = useDeviceStore()
const switchgearStore = useSwitchgearStore()
const sequenceStore = useSequenceStore()
const validation = useValidationStore()

const props = defineProps<{
  errors: ValidationError[]
}>()

defineEmits<{
  (e: "focus-field", err: ValidationError): void
}>()

const filteredErrors = computed(() =>
  validation.showErrorsOnly
    ? props.errors.filter(e => e.level !== "warning")
    : props.errors
)


// rerun validation when entities change
watch(
  () => [
    deviceStore.devices,
    switchgearStore.switchgears,
    sequenceStore.sequences,
  ],
  async () => {
    await runGlobalValidation()
  },
  { deep: true, immediate: true }
)
</script>
