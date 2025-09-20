<template>
  <div class="h-full w-full flex flex-col">
    <div class="flex-1 overflow-y-auto p-3 text-xs text-neutral-700 dark:text-neutral-300">
      <ul class="list-disc pl-4 space-y-1">
        <li
          v-for="(err, i) in errors"
          :key="i"
          class="cursor-pointer hover:underline"
          @click="$emit('focus-field', err)"
        >
          [{{ err.schemaName }} #{{ err.itemId }}] – {{ err.message }}
        </li>
      </ul>
      <div v-if="!errors.length" class="text-neutral-500">
        No validation errors
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { runGlobalValidation } from "@/property-schemas/runValidation";
import type { ValidationError } from "@/property-schemas/validation"
import { useDeviceStore } from "@/stores/deviceStore";
import { useSequenceStore } from "@/stores/sequenceStore";
import { useSwitchgearStore } from "@/stores/switchgearStore";
import { watch } from "vue";

const deviceStore = useDeviceStore()
const switchgearStore = useSwitchgearStore()
const sequenceStore = useSequenceStore()

defineProps<{
  errors: ValidationError[]
}>()

defineEmits<{
  (e: "focus-field", err: ValidationError): void
}>()

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
