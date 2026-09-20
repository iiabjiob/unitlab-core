<script setup lang="ts">
import { computed } from "vue"
import { UiMenu, UiMenuContent, UiMenuItem, UiMenuTrigger } from "@affino/menu-vue"

type ListboxValue = string | number | null
type ListboxOption = { value: ListboxValue; label: string; disabled?: boolean }

const props = withDefaults(defineProps<{
  modelValue?: ListboxValue
  options: ListboxOption[]
  placeholder?: string
  disabled?: boolean
  ariaLabel?: string
  name?: string
  panelWidth?: string
  compactOptions?: boolean
}>(), {
  modelValue: null,
  placeholder: "Select",
  disabled: false,
  ariaLabel: "Listbox",
  name: undefined,
  panelWidth: undefined,
  compactOptions: false,
})

const emit = defineEmits<{
  (event: "update:modelValue", value: ListboxValue): void
  (event: "change", value: ListboxValue): void
  (event: "blur"): void
}>()

const normalizedOptions = computed(() => props.options ?? [])
const selectedOption = computed(() => normalizedOptions.value.find(option => valuesEqual(option.value, props.modelValue)) ?? null)
const hiddenInputValue = computed(() => props.modelValue === null || typeof props.modelValue === "undefined" ? "" : String(props.modelValue))

function valuesEqual(left: ListboxValue | undefined, right: ListboxValue | undefined): boolean {
  if (left === null || typeof left === "undefined" || right === null || typeof right === "undefined") {
    return left === right
  }
  return String(left) === String(right)
}

function optionId(index: number): string {
  return `ui-affino-listbox-option-${index}`
}

function commitOption(option: ListboxOption): void {
  if (option.disabled) return
  emit("update:modelValue", option.value)
  emit("change", option.value)
}
</script>

<template>
  <div class="ui-affino-listbox" :class="{ 'ui-affino-listbox--compact': props.compactOptions }">
    <UiMenu
      placement="bottom"
      align="start"
      :gutter="6"
      :options="{ closeOnSelect: true, loopFocus: true }"
    >
      <UiMenuTrigger as-child>
        <button
          type="button"
          class="ui-affino-listbox__trigger"
          :aria-label="props.ariaLabel"
          aria-haspopup="listbox"
          :disabled="props.disabled"
        >
          <slot name="trigger">
            <span class="ui-affino-listbox__label" :class="{ 'ui-affino-listbox__label--selected': selectedOption }">
              <template v-if="selectedOption">{{ selectedOption.label }}</template>
              <slot v-else name="placeholder">{{ props.placeholder }}</slot>
            </span>
          </slot>
          <span class="ui-affino-listbox__chevron" aria-hidden="true">▾</span>
        </button>
      </UiMenuTrigger>

      <UiMenuContent
        class="ui-affino-listbox__panel"
        :style="{ '--ui-menu-min-width': 'max-content', '--ui-menu-max-width': 'min(24rem, calc(100vw - 1rem))' }"
      >
        <UiMenuItem
          v-for="(option, index) in normalizedOptions"
          :id="optionId(index)"
          :key="`${typeof option.value}:${String(option.value)}`"
          :disabled="option.disabled"
          class="ui-affino-listbox__option"
          :class="{ 'ui-affino-listbox__option--selected': valuesEqual(option.value, props.modelValue) }"
          @select="commitOption(option)"
        >
          <span>{{ option.label }}</span>
          <span v-if="valuesEqual(option.value, props.modelValue)" aria-hidden="true">✓</span>
        </UiMenuItem>
        <div v-if="!normalizedOptions.length" class="ui-affino-listbox__empty">No options</div>
      </UiMenuContent>
    </UiMenu>

    <input v-if="props.name" :name="props.name" type="hidden" autocomplete="off" :value="hiddenInputValue">
  </div>
</template>

<style scoped>
.ui-affino-listbox {
  position: relative;
  width: 100%;
}

.ui-affino-listbox__trigger {
  position: relative;
  width: 100%;
  padding: 0.5rem 2.25rem 0.5rem 0.75rem;
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-lg);
  background: var(--color-white);
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  text-align: left;
  transition: border-color 150ms ease, box-shadow 150ms ease, background 150ms ease;
}

.ui-affino-listbox__trigger:focus-visible {
  outline: none;
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--color-sky-500) 30%, transparent);
}

.ui-affino-listbox__trigger:disabled {
  cursor: not-allowed;
  opacity: 0.6;
}

.ui-affino-listbox__label {
  color: var(--color-neutral-500);
}

.ui-affino-listbox__label--selected {
  color: var(--color-neutral-900);
}

.ui-affino-listbox__chevron {
  position: absolute;
  top: 50%;
  right: 0.75rem;
  color: var(--color-neutral-500);
  pointer-events: none;
  transform: translateY(-50%);
}

.ui-affino-listbox__panel {
  overflow: auto;
}

.ui-affino-listbox__option {
  min-width: var(--ui-menu-min-width);
}

.ui-affino-listbox--compact :deep(.ui-menu-item) {
  padding-block: 0.35rem;
  font-size: var(--text-xs);
}

.ui-affino-listbox__option--selected {
  background: color-mix(in srgb, var(--color-blue-100) 70%, var(--color-white));
  color: var(--color-blue-800);
}

.ui-affino-listbox__empty {
  padding: 0.5rem 0.75rem;
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

:global(.dark .ui-affino-listbox__trigger) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
  color: var(--color-neutral-100);
}

:global(.dark .ui-affino-listbox__label) {
  color: var(--color-neutral-400);
}

:global(.dark .ui-affino-listbox__label--selected) {
  color: var(--color-neutral-100);
}

:global(.dark .ui-affino-listbox__chevron) {
  color: var(--color-neutral-400);
}

:global(.dark .ui-affino-listbox__option--selected) {
  background: color-mix(in srgb, var(--color-blue-500) 20%, transparent);
  color: var(--color-blue-100);
}

:global(.dark .ui-affino-listbox__empty) {
  color: var(--color-neutral-400);
}
</style>
