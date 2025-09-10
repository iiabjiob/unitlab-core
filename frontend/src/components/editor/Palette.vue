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
import { useEditorStore } from "@/stores/editorStore"
import ButtonComponent from "../ui/ButtonComponent.vue";
const editor = useEditorStore()
function addSwitchgear() { editor.addSwitchgear() }
</script>
