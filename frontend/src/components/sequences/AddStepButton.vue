<!-- src/components/sequences/AddStepControls.vue -->
<template>
  <div class="flex items-center gap-2 mt-4">
    <UiButton size="xs" type="dashed" @click="addDefault">
      + Add Step
    </UiButton>

  </div>
</template>

<script setup lang="ts">

import UiButton from "../ui/UiButton.vue"
import type { StepKind } from "@/types/sequences"

const props = defineProps<{
  kinds: StepKind[]
}>()

const emit = defineEmits<{
  (e: "add", kind: StepKind): void
}>()

// Default = WAIT 500ms
function addDefault() {
  emit("add", "WAIT" as StepKind)
}
</script>
