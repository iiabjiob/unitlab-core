<script setup lang="ts">
defineProps<{
  label: string
  description?: string | null
  active?: boolean
}>()
</script>

<template>
  <div
    class="settings-list-item"
    :class="active ? 'settings-list-item--active' : 'settings-list-item--idle'"
  >
    <div class="settings-list-item__label">{{ label }}</div>
    <div v-if="description" class="settings-list-item__description">
      {{ description }}
    </div>
  </div>
</template>

<style scoped>
.settings-list-item {
  user-select: none;
  padding: 0.5rem 0.75rem;
  border-radius: 0.75rem;
  transition: background-color 120ms ease, color 120ms ease;
}

.settings-list-item--active {
  background: var(--color-neutral-100);
  color: var(--color-neutral-900);
}

.settings-list-item--idle {
  color: var(--color-neutral-700);
}

.settings-list-item--idle:hover {
  background: var(--color-neutral-50);
}

.settings-list-item__label {
  font-size: var(--text-sm);
  font-weight: 500;
  line-height: 1.25;
}

.settings-list-item__description {
  margin-top: 0.125rem;
  color: var(--color-neutral-500);
  font-size: 11px;
  line-height: 1.375;
}

:global(.dark) .settings-list-item--active {
  background: var(--color-neutral-800);
  color: var(--color-neutral-100);
}

:global(.dark) .settings-list-item--idle {
  color: var(--color-neutral-300);
}

:global(.dark) .settings-list-item--idle:hover {
  background: var(--color-neutral-900);
}

:global(.dark) .settings-list-item__description {
  color: var(--color-neutral-400);
}
</style>
