<template>

  <div class="p-3">
    <SwitchgearsToolbar
      @add="onAdd"
      @save="onSave"
      @load="onLoad"
    />
  </div>

  <div class="flex-1 overflow-auto p-3">

    <div class="flex flex-wrap gap-4">
      <div
        v-for="item in switchgearStore.switchgears"
        :key="item.id"
        class="relative w-[280px] flex-shrink-0"
      >
        <SelectableCard class="w-full">
          <SwitchgearCard
            :id="item.id"
            :title="item.title"
            :do_open="item.do_open"
            :do_closed="item.do_closed"
            :di_open="item.di_open"
            :di_close="item.di_close"
            @delete="onDelete(item)"
          />
        </SelectableCard>
      </div>
    </div>

    <!-- empty state -->
    <div v-if="!switchgearStore.switchgears.length" class="text-neutral-400">
      No switchgears yet. Create or import one
    </div>
  </div>
</template>

<script setup lang="ts">
import { useSwitchgearStore } from "@/stores/switchgearStore"
import type { Switchgear } from "@/types/switchgear"

import SwitchgearCard from "@/components/switchgear/SwitchgearCard.vue"
import SelectableCard from "@/components/ui/SelectableCard.vue"
import SwitchgearsToolbar from "@/components/toolbars/SwitchgearsToolbar.vue"

const switchgearStore = useSwitchgearStore()

async function onAdd() {
  const sg = await switchgearStore.create({
    kind: "switchgear",
    title: "Switchgear",
    do_open: null,
    do_closed: null,
    di_open: null,
    di_close: null,
    feedback_delay_ms: 0,
  })
}

async function onSave() {
  //
}

async function onLoad() {
  //
}

function onDelete(item: Switchgear) {
  if (confirm(`Delete Switchgear ${item.title}?`)) {
    switchgearStore.remove(item.id)
  }
}
</script>
