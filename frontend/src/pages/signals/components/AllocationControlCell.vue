<template>
  <div class="allocation-control-cell">
    <button
      v-if="mode === 'do'"
      type="button"
      class="allocation-control-cell__do-button"
      tabindex="-1"
      :disabled="disabled"
      :aria-label="ariaLabel ?? `Control: ${stateLabel}`"
      :aria-pressed="ariaPressed"
      @click.stop="activate()"
      @keydown.enter.stop.prevent="activate()"
      @keydown.space.stop.prevent="activate()"
    >
      <span
        class="allocation-control-cell__switch"
        :class="isOn ? 'allocation-control-cell__switch--on' : 'allocation-control-cell__switch--off'"
      >
        <span
          class="allocation-control-cell__switch-thumb"
          :class="isOn ? 'allocation-control-cell__switch-thumb--on' : 'allocation-control-cell__switch-thumb--off'"
        ></span>
      </span>
      <span class="allocation-control-cell__inline-status">
        <span class="allocation-control-cell__lamp" :class="lampTone"></span>
        <span v-if="statusTag" class="allocation-control-cell__status allocation-control-cell__status--do" :class="statusTone">{{ statusTag }}</span>
      </span>
    </button>

    <div
      v-else-if="mode === 'ao'"
      class="allocation-control-cell__ao"
      @click.stop
      @mousedown.stop
      @pointerdown.stop
      @keydown.stop
    >
      <template v-if="aoActive">
        <span class="allocation-control-cell__inline-status allocation-control-cell__inline-status--compact">
          <span class="allocation-control-cell__lamp" :class="lampTone"></span>
          <span v-if="statusTag" class="allocation-control-cell__status allocation-control-cell__status--badge" :class="statusTone" :title="statusTitle">{{ statusTag }}</span>
        </span>
        <input
          :value="aoInputValue"
          type="number"
          inputmode="decimal"
          min="4"
          max="20"
          step="0.1"
          autocomplete="off"
          class="allocation-control-cell__ao-input"
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
        <span class="allocation-control-cell__unit">mA</span>
        <button
          type="button"
          class="allocation-control-cell__mini-button allocation-control-cell__mini-button--commit"
          :disabled="disabled || aoPending"
          @mousedown.stop
          @pointerdown.stop
          @click.stop="commitAoEdit()"
        >
          {{ aoPending ? 'Wait' : 'Set' }}
        </button>
        <button
          type="button"
          class="allocation-control-cell__mini-button allocation-control-cell__mini-button--cancel"
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
        class="allocation-control-cell__ao-open"
        tabindex="-1"
        :disabled="disabled || aoPending"
        :aria-label="ariaLabel ?? `Set analog output to ${aoValueLabel}`"
        :title="aoOpenHint || undefined"
        @click.stop.prevent="beginAoEdit()"
        @mousedown.stop
        @pointerdown.stop
      >
        <span class="allocation-control-cell__lamp" :class="lampTone"></span>
        <span v-if="aoValueLabel" class="allocation-control-cell__ao-value">{{ aoValueLabel }}</span>
        <span v-if="aoValueLabel" class="allocation-control-cell__unit">mA</span>
      </button>
      <span
        v-if="statusTag"
        class="allocation-control-cell__status allocation-control-cell__status--badge"
        :class="statusTone"
        :title="statusTitle"
      >
        {{ statusTag }}
      </span>
    </div>

    <span v-else class="allocation-control-cell__empty">—</span>
  </div>
</template>

<script setup lang="ts">
withDefaults(defineProps<{
  mode: "none" | "do" | "ao"
  lampTone?: string
  statusTone?: string
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
  lampTone: "",
  statusTone: "",
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

<style scoped>
.allocation-control-cell {
  align-items: center;
  display: flex;
  height: 100%;
  justify-content: center;
}

.allocation-control-cell__do-button {
  align-items: center;
  color: var(--color-neutral-700);
  display: inline-flex;
  font-size: var(--text-xs);
  font-weight: 600;
  gap: 0.375rem;
  justify-content: flex-start;
}

.allocation-control-cell__do-button:disabled,
.allocation-control-cell__ao-open:disabled,
.allocation-control-cell__mini-button:disabled {
  cursor: not-allowed;
  opacity: 0.6;
}

.allocation-control-cell__switch {
  align-items: center;
  border: 1px solid;
  border-radius: 999px;
  display: inline-flex;
  flex-shrink: 0;
  height: 1rem;
  position: relative;
  transition: background-color 0.1s ease-out, border-color 0.1s ease-out;
  width: 2rem;
}

.allocation-control-cell__switch--on {
  background: color-mix(in srgb, var(--color-emerald-500) 80%, transparent);
  border-color: var(--color-emerald-500);
}

.allocation-control-cell__switch--off {
  background: var(--color-neutral-300);
  border-color: var(--color-neutral-400);
}

.allocation-control-cell__switch-thumb {
  background: var(--color-white);
  border-radius: 999px;
  height: 0.875rem;
  position: absolute;
  transition: transform 0.1s ease-out;
  width: 0.875rem;
}

.allocation-control-cell__switch-thumb--on {
  transform: translateX(1rem);
}

.allocation-control-cell__switch-thumb--off {
  transform: translateX(0.125rem);
}

.allocation-control-cell__inline-status {
  align-items: center;
  display: inline-flex;
  gap: 0.25rem;
  white-space: nowrap;
}

.allocation-control-cell__inline-status--compact {
  gap: 0.25rem;
}

.allocation-control-cell__lamp {
  border-radius: 999px;
  flex-shrink: 0;
  height: 0.5rem;
  width: 0.5rem;
}

.allocation-control-cell__lamp--unknown,
.allocation-control-cell__lamp--off {
  background: var(--color-neutral-400);
}

.allocation-control-cell__lamp--offline {
  background: var(--color-neutral-500);
}

.allocation-control-cell__lamp--pending {
  animation: allocation-control-cell-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
  background: var(--color-amber-400);
}

.allocation-control-cell__lamp--error {
  animation: allocation-control-cell-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
  background: var(--color-red-500);
}

.allocation-control-cell__lamp--fault {
  background: var(--color-red-500);
}

.allocation-control-cell__lamp--ao {
  background: var(--color-sky-500);
}

.allocation-control-cell__lamp--on {
  background: var(--color-emerald-500);
}

.allocation-control-cell__status {
  font-weight: 600;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.allocation-control-cell__status--do {
  display: inline-flex;
  font-size: 10px;
  justify-content: center;
  min-width: 4ch;
}

.allocation-control-cell__status--badge {
  border: 1px solid currentColor;
  border-radius: var(--radius-sm);
  display: inline-flex;
  font-size: 9px;
  justify-content: center;
  min-width: 3ch;
  padding: 0.125rem 0.25rem;
}

.allocation-control-cell__status--muted,
.allocation-control-cell__status--unknown,
.allocation-control-cell__status--off {
  color: var(--color-neutral-500);
}

.allocation-control-cell__status--pending {
  color: var(--color-amber-700);
}

.allocation-control-cell__status--error {
  color: var(--color-red-700);
}

.allocation-control-cell__status--ao {
  color: var(--color-blue-600);
}

.allocation-control-cell__status--on {
  color: var(--color-emerald-600);
}

.allocation-control-cell__ao {
  align-items: center;
  display: flex;
  gap: 0.375rem;
  justify-content: center;
}

.allocation-control-cell__ao-input {
  background: color-mix(in srgb, var(--color-white) 95%, transparent);
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-md);
  box-shadow: var(--shadow-sm);
  color: var(--color-neutral-900);
  font-size: var(--text-xs);
  font-weight: 600;
  outline: none;
  padding: 0.25rem 0.5rem;
  text-align: right;
  transition: background-color 0.15s, border-color 0.15s, box-shadow 0.15s;
  width: 5rem;
}

.allocation-control-cell__ao-input::placeholder {
  color: var(--color-neutral-400);
}

.allocation-control-cell__ao-input:hover {
  border-color: var(--color-neutral-400);
}

.allocation-control-cell__ao-input:focus {
  background: var(--color-white);
  border-color: var(--color-sky-500);
  box-shadow: 0 4px 6px -1px rgb(0 0 0 / 0.1), 0 2px 4px -2px rgb(0 0 0 / 0.1);
}

.allocation-control-cell__unit {
  color: var(--color-neutral-500);
  font-size: 10px;
  font-weight: 500;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.allocation-control-cell__mini-button {
  align-items: center;
  border: 1px solid;
  border-radius: var(--radius-md);
  box-shadow: var(--shadow-sm);
  display: inline-flex;
  font-size: 9px;
  font-weight: 600;
  height: 1.25rem;
  letter-spacing: 0.06em;
  padding-inline: 0.375rem;
  text-transform: uppercase;
  transition: background-color 0.15s, border-color 0.15s, color 0.15s, opacity 0.15s;
}

.allocation-control-cell__mini-button--commit {
  background: color-mix(in srgb, var(--color-emerald-50) 90%, transparent);
  border-color: var(--color-emerald-300);
  color: var(--color-emerald-700);
}

.allocation-control-cell__mini-button--commit:hover:not(:disabled) {
  background: var(--color-emerald-50);
  border-color: var(--color-emerald-400);
}

.allocation-control-cell__mini-button--cancel {
  background: color-mix(in srgb, var(--color-white) 90%, transparent);
  border-color: var(--color-neutral-300);
  color: var(--color-neutral-600);
}

.allocation-control-cell__mini-button--cancel:hover:not(:disabled) {
  background: var(--color-neutral-100);
  border-color: var(--color-neutral-400);
}

.allocation-control-cell__ao-open {
  align-items: center;
  border: 1px solid transparent;
  border-radius: var(--radius-sm);
  color: var(--color-neutral-700);
  display: inline-flex;
  font-size: var(--text-xs);
  font-weight: 500;
  gap: 0.375rem;
  padding: 0.25rem 0.375rem;
  transition: background-color 0.15s, border-color 0.15s, color 0.15s, opacity 0.15s;
}

.allocation-control-cell__ao-open:hover:not(:disabled) {
  background: var(--color-neutral-50);
  border-color: var(--color-neutral-200);
}

.allocation-control-cell__ao-value {
  font-variant-numeric: tabular-nums;
}

.allocation-control-cell__empty {
  color: var(--color-neutral-400);
  font-size: var(--text-xs);
}

:global(.dark .allocation-control-cell__do-button),
:global(.dark .allocation-control-cell__ao-open) {
  color: var(--color-neutral-200);
}

:global(.dark .allocation-control-cell__switch--off) {
  background: var(--color-neutral-700);
  border-color: var(--color-neutral-600);
}

:global(.dark .allocation-control-cell__lamp--unknown),
:global(.dark .allocation-control-cell__lamp--off) {
  background: var(--color-neutral-600);
}

:global(.dark .allocation-control-cell__lamp--offline) {
  background: var(--color-neutral-700);
}

:global(.dark .allocation-control-cell__status--muted),
:global(.dark .allocation-control-cell__status--off) {
  color: var(--color-neutral-400);
}

:global(.dark .allocation-control-cell__status--unknown) {
  color: var(--color-neutral-300);
}

:global(.dark .allocation-control-cell__status--pending) {
  color: var(--color-amber-300);
}

:global(.dark .allocation-control-cell__status--error) {
  color: var(--color-red-300);
}

:global(.dark .allocation-control-cell__status--ao) {
  color: var(--color-blue-300);
}

:global(.dark .allocation-control-cell__status--on) {
  color: var(--color-emerald-300);
}

:global(.dark .allocation-control-cell__ao-input) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-100);
}

:global(.dark .allocation-control-cell__ao-input:hover) {
  border-color: var(--color-neutral-600);
}

:global(.dark .allocation-control-cell__ao-input:focus) {
  background: var(--color-neutral-900);
}

:global(.dark .allocation-control-cell__unit) {
  color: var(--color-neutral-400);
}

:global(.dark .allocation-control-cell__mini-button--commit) {
  background: transparent;
  border-color: color-mix(in srgb, var(--color-emerald-500) 60%, transparent);
  color: var(--color-emerald-300);
}

:global(.dark .allocation-control-cell__mini-button--commit:hover:not(:disabled)) {
  background: color-mix(in srgb, var(--color-emerald-900) 40%, transparent);
}

:global(.dark .allocation-control-cell__mini-button--cancel) {
  background: transparent;
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-300);
}

:global(.dark .allocation-control-cell__mini-button--cancel:hover:not(:disabled)) {
  background: var(--color-neutral-800);
}

:global(.dark .allocation-control-cell__ao-open:hover:not(:disabled)) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
}

@keyframes allocation-control-cell-pulse {
  50% {
    opacity: 0.5;
  }
}
</style>
