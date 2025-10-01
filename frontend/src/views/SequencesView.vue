<template>
  <div class="p-3">
    <SequencesToolbar
      :importing="importing"
      @add="onAdd"
      @import="openFileDialog"
    />
    <input
      ref="fileInput"
      type="file"
      accept="application/json"
      class="hidden"
      @change="onFileSelected"
    />
  </div>
  <div class="flex-1 p-3">
    <div class="flex flex-wrap gap-4">
      <div v-for="seq in store.sequences" :key="seq.id" class="relative w-full flex-shrink-0">
        <SelectableCard :selected="selection.isSelected('sequence', seq)" @click="select(seq)">
          <SequenceCard
            :sequence="seq"
            @export="onExport"
            @delete="onDelete"
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
import { useSequenceStore } from "@/stores/sequenceStore"
import { useSelectionStore } from "@/stores/selectionStore"
import SequenceCard from "@/components/sequences/SequenceCard.vue"
import SelectableCard from "@/components/ui/SelectableCard.vue"
import type { SequenceDef } from "@/types/sequences"
import SequencesToolbar from "@/components/toolbars/SequencesToolbar.vue"
import { useSequenceImport } from "@/composables/useSequenceImport"

const store = useSequenceStore()
const selection = useSelectionStore()

const { importing, fileInput, openFileDialog, onFileSelected } = useSequenceImport()

function select(item: SequenceDef) {
  selection.select({ type: "sequence", key: item.id })
}

function onExport(seq: SequenceDef) {
  window.open(`/api/sequences/${seq.id}/export-file`, "_blank")
}

async function onAdd() {
  const seq = await store.createSequence({
    name: "New Sequence",
    description: "Draft sequence",
  })
  selection.select({ type: "sequence", key: seq.id })
}

async function onDelete(seq: SequenceDef) {
  if (confirm(`Delete Sequence ${seq.name}?`)) {
    await store.deleteSequence(seq.id)
    if (selection.selected?.type === "sequence" && selection.selected.key === seq.id) {
      selection.clear()
    }
  }
}
</script>
