<!-- src/components/editor/Workspace.vue -->
<template>

  <div class="flex-1 overflow-auto p-4">
    <div class="flex flex-wrap gap-4">
      <div v-for="item in editor.items" :key="item.id" class="relative w-[320px] flex-shrink-0">
        <SwitchgearCard
          class="cursor-pointer"
          :title="item.title"
          :do-unit-id="item.doUnitId ?? ''"
          :do-open-ch="item.doOpenCh ?? 0"
          :do-close-ch="item.doCloseCh ?? 1"
          :di-unit-id="item.diUnitId ?? ''"
          :di-open-pulse-ch="item.diOpenPulseCh ?? 0"
          :di-close-pulse-ch="item.diClosePulseCh ?? 1"
          :selected="selection.isSelected('switchgear', item)"
          @click="select(item)"
        />
      </div>
    </div>

    <!-- empty state -->
    <div v-if="!editor.items.length" class="text-neutral-400">
      Drop or add a Switchgear from the palette…
    </div>
  </div>

</template>

<script setup lang="ts">

import { useEditorStore } from "@/stores/editorStore"
import { useSelectionStore } from "@/stores/selectionStore"
import type { Switchgear } from "@/types/switchgear"
import SwitchgearCard from "../switchgear/SwitchgearCard.vue"

const editor = useEditorStore()
const selection = useSelectionStore()

function select(item: Switchgear) {
  selection.select({ type: "switchgear", id: item.id })
}
function remove(id: string) {
  editor.removeById(id)
  if (selection.selected?.type === "switchgear" && selection.selected.id === id) {
    selection.clear()
  }
}
</script>
