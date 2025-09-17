<template>
  <UiToolbar title="Switchgears">
    <template #left>
      <UiButton type="toolbar" size="xs" @click="onAdd" >
        ➕ Add
      </UiButton>
      <UiButton type="toolbar" size="sm">💾 Save</UiButton>
      <UiButton type="toolbar" size="sm">📂 Load</UiButton>
    </template>
  </UiToolbar>
</template>

<script setup lang="ts">
import UiToolbar from "@/components/ui/UiToolbar.vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import UiButton from "../ui/UiButton.vue"

const store = useSwitchgearStore()

async function onAdd() {
  try {
    await store.create({
      kind: "switchgear",    // дефолтный тип
      title: "Switchgear",
      do_open: null,
      do_closed: null,
      di_open: null,
      di_close: null,
      feedback_delay_ms: 0,  // или, например, 500
    })
  } catch (err) {
    console.error("💥 Failed to add switchgear", err)
  }
}
</script>
