<template>
  <div class="flex h-full items-center justify-center">
    <button
      v-if="canControl"
      type="button"
      class="inline-flex items-center justify-start gap-1.5 text-xs font-semibold text-neutral-700 disabled:cursor-not-allowed disabled:opacity-60 dark:text-neutral-200"
      tabindex="-1"
      :disabled="disabled"
      :aria-label="ariaLabel ?? `Control: ${stateLabel}`"
      :aria-pressed="ariaPressed"
      @click.stop="activate()"
      @keydown.enter.stop.prevent="activate()"
      @keydown.space.stop.prevent="activate()"
    >
      <span
        class="relative inline-flex h-4 w-8 shrink-0 items-center rounded-full border transition-colors duration-100 ease-out"
        :class="isOn ? 'border-emerald-500 bg-emerald-500/80' : 'border-neutral-400 bg-neutral-300 dark:border-neutral-600 dark:bg-neutral-700'"
      >
        <span
          class="absolute h-3.5 w-3.5 rounded-full bg-white transition-transform duration-100 ease-out"
          :class="isOn ? 'translate-x-4' : 'translate-x-0.5'"
        ></span>
      </span>
      <span class="inline-flex items-center gap-1 whitespace-nowrap">
        <span class="h-2 w-2 shrink-0 rounded-full" :class="lampClass"></span>
        <span v-if="statusTag" class="inline-flex min-w-[4ch] justify-center text-[10px] uppercase tracking-[0.08em]" :class="statusClass">{{ statusTag }}</span>
      </span>
    </button>
    <span v-else class="text-xs text-neutral-400">—</span>
  </div>
</template>

<script setup lang="ts">
const props = defineProps<{
  canControl: boolean
  lampClass: string
  statusClass: string
  statusTag: string
  stateLabel: string
  disabled: boolean
  isOn: boolean
  activate: () => void
  ariaLabel?: string
  ariaPressed?: "true" | "false" | "mixed"
}>()
</script>