<!-- src/views/SwitchgearEditorView.vue -->
<template>
  <div class="h-dvh flex bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200">
    <Palette />

    <div class="flex-1 flex min-w-0">
      <Workspace />
    </div>
  </div>
</template>

<script setup lang="ts">
import Palette from "@/components/editor/Palette.vue"
import Workspace from "@/components/editor/Workspace.vue"

</script>
