<template>
  <div class="flex w-full items-center justify-between gap-2 px-1">
    <span class="min-w-0 flex items-center gap-1.5" :title="label">
      <span
        class="h-1.5 w-1.5 shrink-0 rounded-full"
        :class="indicatorClass"
        aria-hidden="true"
      ></span>
      <span class="truncate text-xs font-medium" :class="labelClass">{{ label }}</span>
    </span>

    <button
      type="button"
      class="shrink-0 rounded-md px-2 py-1 text-[10px] font-semibold uppercase tracking-[0.08em] transition-colors"
      :class="buttonClass"
      tabindex="-1"
      :disabled="disabled"
      :aria-label="ariaLabel"
      @mousedown.stop
      @click.stop.prevent="activate()"
      @keydown.enter.stop.prevent="activate()"
      @keydown.space.stop.prevent="activate()"
    >
      {{ actionLabel }}
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
  if (!props.assigned) return "bg-neutral-300 dark:bg-neutral-600"
  if (props.online === true) return "bg-emerald-500"
  if (props.online === false) return "bg-amber-500"
  return "bg-sky-500"
})

const buttonClass = computed(() => {
  if (props.disabled) {
    return "cursor-not-allowed text-neutral-400 opacity-60 dark:text-neutral-500"
  }
  if (props.active) {
    return "border border-sky-200 bg-sky-50 text-sky-800 shadow-sm dark:border-sky-800 dark:bg-sky-900/40 dark:text-sky-100"
  }
  return "text-neutral-500 hover:bg-neutral-100 hover:text-neutral-700 dark:text-neutral-400 dark:hover:bg-neutral-800 dark:hover:text-neutral-200"
})

const labelClass = computed(() => {
  if (!props.assigned) return "text-neutral-500 dark:text-neutral-400"
  if (props.online === true) return "text-neutral-900 dark:text-neutral-100"
  return "text-neutral-700 dark:text-neutral-300"
})

</script>