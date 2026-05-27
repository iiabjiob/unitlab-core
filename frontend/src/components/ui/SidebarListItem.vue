<script setup lang="ts">
import { computed } from "vue"

defineOptions({ inheritAttrs: false })

const props = withDefaults(defineProps<{
  active?: boolean
  disabled?: boolean
}>(), {
  active: false,
  disabled: false,
})

const emit = defineEmits<{ (e: "select"): void }>()

const rootClasses = computed(() => [
  "sidebar-list-item",
  props.disabled ? "is-disabled" : null,
  props.active ? "is-active" : null,
])

function handleClick(event: MouseEvent) {
  if (props.disabled) return
  emit("select")
}
</script>

<template>
  <div
    v-bind="$attrs"
    :class="rootClasses"
    @click="handleClick"
  >
    <div class="sidebar-list-item__indicator" />

    <div class="sidebar-list-item__body">
      <div class="sidebar-list-item__main">
        <slot name="prefix" />
        <div class="sidebar-list-item__label">
          <slot />
        </div>
        <slot name="suffix" />
      </div>

      <div v-if="$slots.subtitle" class="sidebar-list-item__subtitle">
        <slot name="subtitle" />
      </div>
      <div v-else-if="$slots.meta" class="sidebar-list-item__meta">
        <slot name="meta" />
      </div>
    </div>
  </div>
</template>

<style scoped>
.sidebar-list-item {
  align-items: center;
  color: var(--color-neutral-400);
  cursor: pointer;
  display: flex;
  padding: 0.5rem 0.75rem;
  transition: background-color 150ms ease, color 150ms ease, opacity 150ms ease;
  user-select: none;
}

.sidebar-list-item:hover {
  background: color-mix(in srgb, var(--color-neutral-900) 12%, transparent);
}

.sidebar-list-item.is-disabled {
  cursor: not-allowed;
  opacity: 0.6;
}

.sidebar-list-item.is-active {
  color: var(--color-neutral-900);
}

.sidebar-list-item__indicator {
  background: transparent;
  border-radius: var(--radius-sm);
  flex: 0 0 auto;
  height: 1.25rem;
  margin-right: 0.5rem;
  transition: background-color 150ms ease;
  width: 0.25rem;
}

.sidebar-list-item:hover .sidebar-list-item__indicator {
  background: var(--color-neutral-700);
}

.sidebar-list-item.is-active .sidebar-list-item__indicator {
  background: var(--color-blue-500);
}

.sidebar-list-item__body {
  flex: 1 1 auto;
  min-width: 0;
}

.sidebar-list-item__main {
  align-items: center;
  display: flex;
  gap: 0.5rem;
  min-width: 0;
}

.sidebar-list-item__label {
  flex: 1 1 auto;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.sidebar-list-item__subtitle {
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  letter-spacing: 0.3em;
  line-height: 1rem;
  margin-top: 0.125rem;
  text-transform: uppercase;
}

.sidebar-list-item__meta {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  line-height: 1rem;
  margin-top: 0.125rem;
}

.dark .sidebar-list-item {
  color: var(--color-neutral-600);
}

.dark .sidebar-list-item.is-active {
  color: var(--color-white);
}

.dark .sidebar-list-item__subtitle,
.dark .sidebar-list-item__meta {
  color: var(--color-neutral-400);
}
</style>
