<script setup lang="ts">
type BrowserObject = {
  id: string
  label: string
  kind: "line" | "symbol" | "text"
  selected: boolean
}

const props = defineProps<{
  objects: BrowserObject[]
  selectionCount: number
  canDelete: boolean
}>()

const emit = defineEmits<{
  (event: "select", id: string, pointerEvent: MouseEvent): void
  (event: "delete"): void
  (event: "close"): void
}>()
</script>

<template>
  <aside class="switchgear-sld-object-browser" aria-label="SLD objects">
    <header class="switchgear-sld-object-browser__header">
      <div>
        <strong>Objects</strong>
        <span>{{ props.objects.length }}</span>
      </div>
      <button type="button" aria-label="Hide objects" @click="emit('close')">×</button>
    </header>

    <div v-if="props.objects.length === 0" class="switchgear-sld-object-browser__empty">
      No added objects
    </div>
    <div v-else class="switchgear-sld-object-browser__list">
      <button
        v-for="item in props.objects"
        :key="item.id"
        type="button"
        class="switchgear-sld-object-browser__item"
        :class="{ 'is-selected': item.selected }"
        @click="emit('select', item.id, $event)"
      >
        <span class="switchgear-sld-object-browser__kind">{{ item.kind === 'line' ? '━' : item.kind === 'symbol' ? '◇' : 'T' }}</span>
        <span class="switchgear-sld-object-browser__label">{{ item.label }}</span>
      </button>
    </div>

    <footer class="switchgear-sld-object-browser__footer">
      <span>{{ props.selectionCount }} selected</span>
      <button type="button" :disabled="!props.canDelete" @click="emit('delete')">Delete</button>
    </footer>
  </aside>
</template>

<style scoped>
.switchgear-sld-object-browser {
  position: absolute;
  top: 0.75rem;
  left: 0.75rem;
  z-index: 4;
  display: flex;
  width: min(17rem, calc(100% - 1.5rem));
  max-height: min(32rem, calc(100% - 1.5rem));
  flex-direction: column;
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-lg);
  background: color-mix(in srgb, var(--color-white) 94%, transparent);
  box-shadow: var(--shadow-lg);
  backdrop-filter: blur(12px);
}

.switchgear-sld-object-browser__header,
.switchgear-sld-object-browser__footer {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5rem;
  padding: 0.55rem 0.7rem;
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
}

.switchgear-sld-object-browser__header {
  border-bottom: 1px solid var(--color-neutral-200);
}

.switchgear-sld-object-browser__header div {
  display: flex;
  gap: 0.45rem;
}

.switchgear-sld-object-browser__header button,
.switchgear-sld-object-browser__footer button {
  border: 0;
  background: transparent;
  color: var(--color-neutral-500);
  font: inherit;
  cursor: pointer;
}

.switchgear-sld-object-browser__footer button {
  color: var(--color-rose-600);
  font-weight: 600;
}

.switchgear-sld-object-browser__footer button:disabled {
  cursor: not-allowed;
  opacity: 0.45;
}

.switchgear-sld-object-browser__list {
  min-height: 0;
  overflow: auto;
  padding: 0.35rem;
}

.switchgear-sld-object-browser__item {
  display: flex;
  width: 100%;
  align-items: center;
  gap: 0.5rem;
  padding: 0.45rem 0.5rem;
  border: 0;
  border-radius: var(--radius-md);
  background: transparent;
  color: var(--color-neutral-700);
  font: inherit;
  font-size: var(--text-xs);
  text-align: left;
  cursor: pointer;
}

.switchgear-sld-object-browser__item:hover,
.switchgear-sld-object-browser__item.is-selected {
  background: var(--color-blue-50);
  color: var(--color-blue-800);
}

.switchgear-sld-object-browser__kind {
  width: 1.2rem;
  color: var(--color-neutral-500);
  text-align: center;
}

.switchgear-sld-object-browser__label {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.switchgear-sld-object-browser__empty {
  padding: 1rem 0.75rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}
</style>
