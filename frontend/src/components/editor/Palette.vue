<!-- src/components/editor/Palette.vue -->
<template>
  <div class="p-3 border-r border-neutral-200 dark:border-neutral-800 w-56 shrink-0">
    <h3 class="text-sm font-semibold mb-2">Elements</h3>

    <button
      class="w-full text-left px-3 py-2 mb-2 rounded bg-neutral-100 dark:bg-neutral-800 hover:bg-neutral-200 dark:hover:bg-neutral-700"
      @click="addSwitchgear"
    >
      ➕ Switchgear
    </button>
  </div>
</template>

<script setup lang="ts">
import { useEditorStore } from "@/stores/editorStore"
const editor = useEditorStore()
function addSwitchgear() { editor.addSwitchgear() }
</script>
