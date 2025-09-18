<template>
  <div class="flex-1 p-3">
    <div class="flex flex-wrap gap-4">
      <div v-for="seq in store.sequences" :key="seq.id" class="relative w-[560px] flex-shrink-0">
        <SelectableCard :selected="selection.isSelected('sequence', seq)" @click="select(seq)">
          <SequenceCard
            :sequence="seq"
            @export="onExport(seq)"
            @delete="remove(seq.id)"
          />
        </SelectableCard>
      </div>
    </div>

    <div v-if="!store.sequences.length" class="text-neutral-400">
      No sequences yet. Create or import one…
    </div>
  </div>
</template>

<script setup lang="ts">
import { onMounted } from "vue"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useSelectionStore } from "@/stores/selectionStore"
import SequenceCard from "@/components/sequences/SequenceCard.vue"
import SelectableCard from "@/components/ui/SelectableCard.vue"
import type { SequenceDef } from "@/types/sequences"

const store = useSequenceStore()
const selection = useSelectionStore()

onMounted(() => {
  store.fetchSequences()
})

function select(item: SequenceDef) {
  selection.select({ type: "sequence", key: item.id })
}

function onExport(seq: SequenceDef) {
  window.open(`/api/sequences/${seq.id}/export-file`, "_blank")
}

async function remove(id: number) {
  await store.deleteSequence(id)
  if (selection.selected?.type === "sequence" && selection.selected.key === id) {
    selection.clear()
  }
}
</script>
