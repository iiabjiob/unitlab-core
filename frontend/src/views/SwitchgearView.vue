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
            :switchgear="item"
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
import { onMounted } from "vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useChannelStore } from "@/stores/channelStore"
import type { Switchgear, SwitchgearBindingInput } from "@/types/switchgear"
import { SWITCHGEAR_BINDING_ROLES } from "@/types/switchgear"

import SwitchgearCard from "@/components/switchgear/SwitchgearCard.vue"
import SelectableCard from "@/components/ui/SelectableCard.vue"
import SwitchgearsToolbar from "@/components/toolbars/SwitchgearsToolbar.vue"

const switchgearStore = useSwitchgearStore()
const channelStore = useChannelStore()

onMounted(() => {
  channelStore.ensureLoaded()
})

async function onAdd() {
  const defaultBindings: SwitchgearBindingInput[] = SWITCHGEAR_BINDING_ROLES.map(role => ({
    role,
    channel_id: null,
    delay_ms: 0,
  }))
  await switchgearStore.create({
    name: "Switchgear",
    switchgear_type: "switchgear",
    bindings: defaultBindings,
  })
}

async function onSave() {
  //
}

async function onLoad() {
  //
}

function onDelete(item: Switchgear) {
  if (confirm(`Delete Switchgear ${item.name}?`)) {
    switchgearStore.remove(item.id)
  }
}
</script>
