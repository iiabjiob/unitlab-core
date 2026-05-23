<template>
  <div class="flex h-full items-center justify-center">
    <button
      v-if="mode === 'do'"
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

    <div
      v-else-if="mode === 'ao'"
      class="flex items-center justify-center gap-1.5"
      @click.stop
      @mousedown.stop
      @pointerdown.stop
      @keydown.stop
    >
      <template v-if="aoActive">
        <span class="inline-flex items-center gap-1">
          <span class="h-2 w-2 shrink-0 rounded-full" :class="lampClass"></span>
          <span v-if="statusTag" class="inline-flex min-w-[3ch] justify-center rounded border px-1 py-0.5 text-[9px] font-semibold uppercase tracking-[0.08em]" :class="statusClass" :title="statusTitle">{{ statusTag }}</span>
        </span>
        <input
          :value="aoInputValue"
          type="number"
          inputmode="decimal"
          min="4"
          max="20"
          step="0.1"
          autocomplete="off"
          class="w-20 rounded-md border border-neutral-300 bg-white/95 px-2 py-1 text-right text-xs font-semibold text-neutral-900 shadow-sm outline-none ring-0 transition placeholder:text-neutral-400 hover:border-neutral-400 focus:border-sky-500 focus:bg-white focus:shadow-md dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100 dark:hover:border-neutral-600 dark:focus:bg-neutral-900"
          :disabled="disabled || aoPending"
          autofocus
          @click.stop
          @mousedown.stop
          @pointerdown.stop
          @focus.stop
          @input="updateAoInput(($event.target as HTMLInputElement).value)"
          @keydown.stop
          @keydown.enter.stop.prevent="commitAoEdit()"
          @keydown.esc.stop.prevent="cancelAoEdit()"
        />
        <span class="text-[10px] font-medium uppercase tracking-[0.08em] text-neutral-500 dark:text-neutral-400">mA</span>
        <button
          type="button"
          class="inline-flex h-5 items-center rounded-md border border-emerald-300 bg-emerald-50/90 px-1.5 text-[9px] font-semibold uppercase tracking-[0.06em] text-emerald-700 shadow-sm transition hover:border-emerald-400 hover:bg-emerald-100 disabled:cursor-not-allowed disabled:opacity-60 dark:border-emerald-500/60 dark:bg-transparent dark:text-emerald-300 dark:hover:bg-emerald-950/40"
          :disabled="disabled || aoPending"
          @mousedown.stop
          @pointerdown.stop
          @click.stop="commitAoEdit()"
        >
          {{ aoPending ? 'Wait' : 'Set' }}
        </button>
        <button
          type="button"
          class="inline-flex h-5 items-center rounded-md border border-neutral-300 bg-white/90 px-1.5 text-[9px] font-semibold uppercase tracking-[0.06em] text-neutral-600 shadow-sm transition hover:border-neutral-400 hover:bg-neutral-100 disabled:cursor-not-allowed disabled:opacity-60 dark:border-neutral-700 dark:bg-transparent dark:text-neutral-300 dark:hover:bg-neutral-800"
          :disabled="aoPending"
          @mousedown.stop
          @pointerdown.stop
          @click.stop="cancelAoEdit()"
        >
          X
        </button>
      </template>
      <button
        v-else
        type="button"
        class="inline-flex items-center gap-1.5 rounded border border-transparent px-1.5 py-1 text-xs font-medium text-neutral-700 transition hover:border-neutral-200 hover:bg-neutral-50 disabled:cursor-not-allowed disabled:opacity-60 dark:text-neutral-200 dark:hover:border-neutral-700 dark:hover:bg-neutral-900"
        tabindex="-1"
        :disabled="disabled || aoPending"
        :aria-label="ariaLabel ?? `Set analog output to ${aoValueLabel}`"
        :title="aoOpenHint || undefined"
        @click.stop.prevent="beginAoEdit()"
        @mousedown.stop
        @pointerdown.stop
      >
        <span class="h-2 w-2 shrink-0 rounded-full" :class="lampClass"></span>
        <span v-if="aoValueLabel" class="tabular-nums">{{ aoValueLabel }}</span>
        <span v-if="aoValueLabel" class="text-[10px] uppercase tracking-[0.08em] text-neutral-500 dark:text-neutral-400">mA</span>
      </button>
      <span
        v-if="statusTag"
        class="inline-flex min-w-[3ch] justify-center rounded border px-1 py-0.5 text-[9px] font-semibold uppercase tracking-[0.08em]"
        :class="statusClass"
        :title="statusTitle"
      >
        {{ statusTag }}
      </span>
    </div>

    <span v-else class="text-xs text-neutral-400">—</span>
  </div>
</template>

<script setup lang="ts">
withDefaults(defineProps<{
  mode: "none" | "do" | "ao"
  lampClass?: string
  statusClass?: string
  statusTag?: string
  statusTitle?: string
  stateLabel?: string
  disabled: boolean
  isOn?: boolean
  activate?: () => void
  ariaLabel?: string
  ariaPressed?: "true" | "false" | "mixed"
  aoActive?: boolean
  aoPending?: boolean
  aoValueLabel?: string
  aoInputValue?: string
  aoOpenHint?: string
  beginAoEdit?: () => void
  cancelAoEdit?: () => void
  commitAoEdit?: () => void
  updateAoInput?: (value: string) => void
}>(), {
  lampClass: "",
  statusClass: "",
  statusTag: "",
  statusTitle: "",
  stateLabel: "",
  isOn: false,
  activate: () => {},
  aoActive: false,
  aoPending: false,
  aoValueLabel: "—",
  aoInputValue: "",
  aoOpenHint: "",
  beginAoEdit: () => {},
  cancelAoEdit: () => {},
  commitAoEdit: () => {},
  updateAoInput: () => {},
})
</script>
