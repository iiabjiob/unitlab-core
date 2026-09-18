<template>
  <div class="editor-workspace-layout workspace-surface">
    <div class="editor-workspace-layout__sidebar-column">
      <ResizablePanel
        v-if="isWideDesktop"
        class="editor-workspace-layout__sidebar-panel"
        placement="left"
        :storage-key="storageKey"
        :min-size="minSize"
        :default-size="defaultSize"
        :max-size="maxSize"
      >
        <slot name="sidebar" />
      </ResizablePanel>
      <div v-else class="editor-workspace-layout__sidebar-card">
        <slot name="sidebar" />
      </div>
    </div>

    <div class="editor-workspace-layout__main-column">
      <slot name="main" />
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useViewport } from "@/composables/useViewport"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"

withDefaults(defineProps<{
  storageKey: string
  minSize?: number
  defaultSize?: number
  maxSize?: number
}>(), {
  minSize: 320,
  defaultSize: 360,
  maxSize: 800,
})

const { width } = useViewport()
const isWideDesktop = computed(() => width.value >= 1500)
</script>

<style scoped>
.editor-workspace-layout {
  display: flex;
  flex: 1 1 0;
  min-width: 0;
  min-height: 0;
  flex-direction: column;
  gap: 1rem;
  margin-top: 1.25rem;
  padding: 1rem;
  border-radius: var(--radius-md);
  overflow: auto;
}

.editor-workspace-layout__sidebar-column,
.editor-workspace-layout__sidebar-panel,
.editor-workspace-layout__main-column {
  display: flex;
  min-width: 0;
  flex-direction: column;
}

.editor-workspace-layout__sidebar-column {
  flex: 0 0 auto;
}

.editor-workspace-layout__sidebar-panel {
  height: 100%;
  min-height: 0;
  flex: 1 1 auto;
  overflow: hidden;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  background: var(--color-neutral-50);
}

.editor-workspace-layout__sidebar-card {
  overflow: hidden;
  padding: 1rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  background: var(--color-neutral-50);
}

.editor-workspace-layout__main-column {
  min-height: 0;
  flex: 1 1 auto;
}

@media (min-width: 640px) {
  .editor-workspace-layout {
    padding: 1.25rem;
  }
}

@media (min-width: 1500px) {
  .editor-workspace-layout {
    flex-direction: row;
    gap: 1.25rem;
    overflow: hidden;
  }

  .editor-workspace-layout__sidebar-column {
    align-self: stretch;
    height: 100%;
    min-height: 0;
  }

  .editor-workspace-layout__main-column {
    flex: 1 1 0;
    overflow: hidden;
  }
}

@media (max-width: 1499px) {
  .editor-workspace-layout__main-column {
    flex: 0 0 auto;
  }
}

:global(.dark .editor-workspace-layout__sidebar-panel),
:global(.dark .editor-workspace-layout__sidebar-card) {
  border-color: var(--color-neutral-700);
  background: color-mix(in srgb, var(--color-neutral-900) 60%, transparent);
}
</style>
