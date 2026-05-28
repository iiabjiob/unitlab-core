<template>
  <div class="allocation-channel-cell">
    <span class="allocation-channel-cell__label-wrap" :title="label">
      <span
        class="allocation-channel-cell__indicator"
        :class="indicatorClass"
        aria-hidden="true"
      ></span>
      <span class="allocation-channel-cell__label" :class="labelClass">{{ label }}</span>
    </span>

    <button
      type="button"
      class="btn btn-xs allocation-channel-cell__button"
      :class="buttonClass"
      tabindex="-1"
      :disabled="disabled"
      :aria-label="ariaLabel"
      :title="actionLabel"
      @mousedown.stop
      @click.stop.prevent="activate()"
      @keydown.enter.stop.prevent="activate()"
      @keydown.space.stop.prevent="activate()"
    >
      <svg
        v-if="assigned"
        class="allocation-channel-cell__button-icon"
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        stroke-width="2"
        stroke-linecap="round"
        stroke-linejoin="round"
        aria-hidden="true"
      >
        <path d="M12 20h9" />
        <path d="M16.5 3.5a2.1 2.1 0 0 1 3 3L7 19l-4 1 1-4Z" />
      </svg>
      <svg
        v-else
        class="allocation-channel-cell__button-icon"
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        stroke-width="2"
        stroke-linecap="round"
        stroke-linejoin="round"
        aria-hidden="true"
      >
        <path d="M10 13a5 5 0 0 0 7.54.54l2-2a5 5 0 0 0-7.07-7.07l-1.15 1.15" />
        <path d="M14 11a5 5 0 0 0-7.54-.54l-2 2a5 5 0 0 0 7.07 7.07l1.15-1.15" />
      </svg>
    </button>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"

const props = withDefaults(defineProps<{
  label: string
  assigned: boolean
  online: boolean | null
  activate: () => void
  ariaLabel?: string
  active?: boolean
  disabled?: boolean
}>(), {
  ariaLabel: undefined,
  active: false,
  disabled: false,
})

const actionLabel = computed(() => {
  return props.assigned ? "Change" : "Assign"
})

const indicatorClass = computed(() => {
  if (!props.assigned) return "allocation-channel-cell__indicator--unassigned"
  if (props.online === true) return "allocation-channel-cell__indicator--online"
  if (props.online === false) return "allocation-channel-cell__indicator--offline"
  return "allocation-channel-cell__indicator--unknown"
})

const buttonClass = computed(() => {
  return props.active ? "btn-primary" : "btn-secondary"
})

const labelClass = computed(() => {
  if (!props.assigned) return "allocation-channel-cell__label--unassigned"
  if (props.online === true) return "allocation-channel-cell__label--online"
  return "allocation-channel-cell__label--offline"
})

</script>

<style scoped>
.allocation-channel-cell {
  align-items: center;
  display: flex;
  gap: 0.5rem;
  justify-content: space-between;
  padding-inline: 0.25rem;
  width: 100%;
}

.allocation-channel-cell__label-wrap {
  align-items: center;
  display: flex;
  gap: 0.375rem;
  min-width: 0;
}

.allocation-channel-cell__indicator {
  border-radius: 999px;
  flex-shrink: 0;
  height: 0.375rem;
  width: 0.375rem;
}

.allocation-channel-cell__indicator--unassigned {
  background: var(--color-neutral-300);
}

.allocation-channel-cell__indicator--online {
  background: var(--color-emerald-500);
}

.allocation-channel-cell__indicator--offline {
  background: var(--color-neutral-400);
}

.allocation-channel-cell__indicator--unknown {
  background: var(--color-sky-500);
}

.allocation-channel-cell__label {
  font-size: var(--text-xs);
  font-weight: 500;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.allocation-channel-cell__label--unassigned {
  color: var(--color-neutral-500);
}

.allocation-channel-cell__label--online {
  color: var(--color-neutral-900);
}

.allocation-channel-cell__label--offline {
  color: var(--color-neutral-700);
}

.allocation-channel-cell__button {
  flex-shrink: 0;
  gap: 0;
  height: 1.5rem;
  line-height: 0;
  min-height: 1.5rem;
  min-width: 1.5rem;
  padding: 0;
  width: 1.5rem;
}

.allocation-channel-cell__button-icon {
  flex-shrink: 0;
  height: 0.875rem;
  width: 0.875rem;
}

:global(.dark .allocation-channel-cell__indicator--unassigned),
:global(.dark .allocation-channel-cell__indicator--offline) {
  background: var(--color-neutral-600);
}

:global(.dark .allocation-channel-cell__label--unassigned) {
  color: var(--color-neutral-400);
}

:global(.dark .allocation-channel-cell__label--online) {
  color: var(--color-neutral-100);
}

:global(.dark .allocation-channel-cell__label--offline) {
  color: var(--color-neutral-300);
}

</style>
