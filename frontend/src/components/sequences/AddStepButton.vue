<!-- src/components/sequences/AddStepControls.vue -->
<template>
  <div class="flex items-center gap-2 mt-4">
    <select
      v-model="selectedKind"
      class="border border-neutral-300 rounded px-2 py-1 text-xs uppercase tracking-wide text-neutral-600 bg-white"
    >
      <option
        v-for="kind in kinds"
        :key="kind"
        :value="kind"
      >
        {{ formatLabel(kind) }}
      </option>
    </select>

    <UiButton size="xs" type="dashed" @click="addSelected">
      + Add Step
    </UiButton>
  </div>
</template>

<script setup lang="ts">
import { ref, watch } from "vue"
import UiButton from "../ui/UiButton.vue"
import { SequenceStepType } from "@/types/sequences"

const props = defineProps<{
  kinds: SequenceStepType[]
}>()

const emit = defineEmits<{
  (e: "add", kind: SequenceStepType): void
}>()

const selectedKind = ref<SequenceStepType>(props.kinds[0] ?? SequenceStepType.WAIT)

watch(
  () => props.kinds,
  (next) => {
    if (!next.includes(selectedKind.value) && next.length) {
      selectedKind.value = next[0]
    }
  },
  { deep: true }
)

function formatLabel(kind: SequenceStepType) {
  return kind.replace(/_/g, " ")
}

function addSelected() {
  emit("add", selectedKind.value)
}
</script>
