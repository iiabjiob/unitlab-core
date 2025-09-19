<template>
  <div class="flex flex-wrap gap-2 mt-4">
    <p class="text-xs font-semibold me-2">Add step:</p>
    <UiButton
      v-for="kind in kinds"
      :key="kind"
      size="xs"
      type="dashed"
      @click.stop="$emit('add', kind)"
    >
      {{ kind }}
    </UiButton>
  </div>
</template>

<script setup lang="ts">
import type { StepKind } from "@/types/sequences";
import UiButton from "../ui/UiButton.vue";

const props = defineProps<{
  kinds: StepKind[]
}>()

const emit = defineEmits<{
  (e: "add", kind: StepKind): void
}>()
</script>
