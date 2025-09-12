<!-- src/components/editor/Palette.vue -->
<template>
  <div class="p-3 border-r border-neutral-200 dark:border-neutral-800 w-56 shrink-0">
    <h3 class="text-sm font-semibold mb-2">Elements</h3>

    <ButtonComponent class="w-full" type="secondary" size="sm" @click="addSwitchgear">
      Add Switchgear
    </ButtonComponent>

  </div>
</template>

<script setup lang="ts">

import { useSwitchgearStore } from "@/stores/switchgearStore";
import ButtonComponent from "../ui/ButtonComponent.vue";

const switchgearStore = useSwitchgearStore()
async function addSwitchgear() {
  try {
    await switchgearStore.create({
      title: "Switchgear",
      kind: "switchgear",
      do_open: null,
      do_closed: null,
      di_open: null,
      di_close: null,
      feedback_delay_ms: 0,
    })
  } catch (err) {
    console.error("❌ Failed to add switchgear", err)
  }
}
</script>
