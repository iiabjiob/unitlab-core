<!-- src/components/editor/Workspace.vue -->
<template>
  <div class="flex-1 overflow-auto p-4">
    <div class="flex flex-wrap gap-4">
      <div
        v-for="item in switchgearStore.switchgears"
        :key="item.id"
        class="relative w-[320px] flex-shrink-0"
      >
        <SelectableCard
          class="w-[320px]"
          :selected="selection.isSelected('switchgear', item)"
          @click="select(item)"
        >
          <SwitchgearCard
            :id="item.id"
            :title="item.title"
            :do_open="item.do_open"
            :do_closed="item.do_closed"
            :di_open="item.di_open"
            :di_close="item.di_close"
            @delete="remove(item.id)"
          />
        </SelectableCard>
      </div>
    </div>

    <!-- empty state -->
    <div v-if="!switchgearStore.switchgears.length" class="text-neutral-400">
      Drop or add a Switchgear from the palette…
    </div>
  </div>
</template>

<script setup lang="ts">
import { onMounted } from "vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useSelectionStore } from "@/stores/selectionStore"
import type { Switchgear } from "@/types/switchgear"

import SwitchgearCard from "../switchgear/SwitchgearCard.vue"
import SelectableCard from "../ui/SelectableCard.vue"

const switchgearStore = useSwitchgearStore()
const selection = useSelectionStore()

onMounted(() => {
  // загружаем список при инициализации
  switchgearStore.fetchAll()
})

function select(item: Switchgear) {
  selection.select({ type: "switchgear", key: item.id })
}

function remove(id: number) {
  switchgearStore.remove(id)
  if (selection.selected?.type === "switchgear" && selection.selected.key === id) {
    selection.clear()
  }
}
</script>
