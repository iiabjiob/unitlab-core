<!-- src/components/ui/UiBadge.vue -->
<template>
  <span
    :class="[
      'ui-badge',
      variantClass,
    ]"
  >
    <slot />
  </span>
</template>

<script setup lang="ts">
import { computed } from "vue"

const props = defineProps<{
  variant?: 'success' | 'danger' | 'neutral' | 'info' | 'warning'
}>()

const variantClass = computed(() => {
  switch (props.variant) {
    case 'success':
      return 'ui-badge--success'
    case 'danger':
      return 'ui-badge--danger'
    case 'info':
      return 'ui-badge--info'
    case 'warning':
      return 'ui-badge--warning'
    default:
      return 'ui-badge--neutral'
  }
})
</script>

<style scoped>
.ui-badge {
  align-items: center;
  border-radius: 0.25rem;
  display: inline-flex;
  font-size: var(--text-xs);
  font-weight: 500;
  line-height: 1rem;
  padding: 0.125rem 0.5rem;
}

.ui-badge--success {
  background: var(--color-green-100);
  color: var(--color-green-800);
}

.ui-badge--danger {
  background: var(--color-red-100);
  color: var(--color-red-800);
}

.ui-badge--info {
  background: var(--color-blue-100);
  color: var(--color-blue-800);
}

.ui-badge--warning {
  background: var(--color-yellow-100);
  color: var(--color-yellow-800);
}

.ui-badge--neutral {
  background: var(--color-neutral-100);
  color: var(--color-neutral-800);
}

.dark .ui-badge--success {
  background: var(--color-green-900);
  color: var(--color-green-300);
}

.dark .ui-badge--danger {
  background: var(--color-red-900);
  color: var(--color-red-300);
}

.dark .ui-badge--info {
  background: var(--color-blue-900);
  color: var(--color-blue-300);
}

.dark .ui-badge--warning {
  background: var(--color-yellow-900);
  color: var(--color-yellow-300);
}

.dark .ui-badge--neutral {
  background: var(--color-neutral-800);
  color: var(--color-neutral-200);
}
</style>
