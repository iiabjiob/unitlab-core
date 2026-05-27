<script setup lang="ts">
import { computed, nextTick, ref, watch } from "vue"
import type { ComponentPublicInstance } from "vue"
import { usePopoverController, useFloatingPopover } from "@affino/popover-vue"
import { createListboxStore, useListboxStore } from "@affino/listbox-vue"

import { runStoreBootstrap } from "@/composables/useStoreBootstrap"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useThemeStore } from "@/stores/themeStore"
import type { SequenceDef } from "@/types/sequences"
import { APP_OVERLAY_HOST_SELECTOR } from "@/utils/overlayHost"

const props = withDefaults(defineProps<{
  modelValue: number | null
  sequences?: SequenceDef[]
  excludedIds?: number[]
  placeholder?: string
  disabled?: boolean
  clearable?: boolean
  id?: string
  name?: string
}>(), {
  sequences: undefined,
  excludedIds: () => [],
  placeholder: "— select instruction —",
  disabled: false,
  clearable: true,
  id: undefined,
  name: undefined,
})

const emit = defineEmits<{
  (e: "update:modelValue", value: number | null): void
  (e: "change", value: number | null): void
}>()

const sequenceStore = useSequenceStore()
const themeStore = useThemeStore()

void runStoreBootstrap(
  ["sequence-picker-combobox"],
  [() => sequenceStore.ensureLoaded()],
  { mode: "settled" },
)

type NormalizedSequence = {
  id: number
  label: string
  description: string
  readOnly: boolean
  system: boolean
  searchable: string
}

const searchQuery = ref("")
const searchInputRef = ref<HTMLInputElement | null>(null)
const listRef = ref<HTMLDivElement | null>(null)
const isDarkTheme = computed(() => themeStore.currentTheme === "dark")

const sourceSequences = computed(() => props.sequences ?? sequenceStore.sequences)
const excludedIdSet = computed(() => new Set(props.excludedIds))

const normalizedSequences = computed<NormalizedSequence[]>(() => (
  (sourceSequences.value ?? [])
    .filter((sequence) => !excludedIdSet.value.has(sequence.id))
    .map((sequence) => {
      const description = String(sequence.description ?? "").trim()
      const searchable = [
        sequence.name,
        description,
        String(sequence.system_key ?? ""),
      ].join(" ").toLowerCase()

      return {
        id: sequence.id,
        label: sequence.name,
        description,
        readOnly: Boolean(sequence.read_only),
        system: Boolean(sequence.system_provided),
        searchable,
      }
    })
    .sort((left, right) => left.label.localeCompare(right.label))
))

const selectedSequence = computed(() =>
  normalizedSequences.value.find((sequence) => sequence.id === props.modelValue) ?? null,
)

const filteredSequences = computed(() => {
  const query = searchQuery.value.trim().toLowerCase()
  const matches = normalizedSequences.value.filter((sequence) =>
    !query.length || sequence.searchable.includes(query),
  )

  if (selectedSequence.value && !matches.some((sequence) => sequence.id === selectedSequence.value?.id)) {
    matches.unshift(selectedSequence.value)
  }

  return matches
})

const emptyStateLabel = computed(() => (
  searchQuery.value.trim().length > 0
    ? "No instructions match your search"
    : "No instructions available"
))

const componentId = `sequence-picker-${Math.random().toString(36).slice(2, 10)}`
const triggerId = computed(() => props.id?.trim() || `${componentId}-trigger`)
const listboxId = `${componentId}-listbox`

const popover = usePopoverController({ closeOnInteractOutside: true })
const floating = useFloatingPopover(popover, {
  strategy: "fixed",
  placement: "bottom",
  align: "start",
  gutter: 6,
  viewportPadding: 8,
  teleportTo: APP_OVERLAY_HOST_SELECTOR,
  zIndex: 1250,
})

const listboxStore = createListboxStore({
  context: {
    optionCount: 0,
    isDisabled: () => false,
  },
})

const { state: listboxState } = useListboxStore(listboxStore)
const activeIndex = computed(() => listboxState.value.activeIndex)

watch(
  computed(() => ({
    optionCount: filteredSequences.value.length,
    isDisabled: (index: number) => !filteredSequences.value[index],
  })),
  (ctx) => {
    listboxStore.setContext(ctx)
    if (ctx.optionCount <= 0) {
      listboxStore.clearSelection({ preserveActiveIndex: false })
      return
    }
    const active = listboxStore.peekState().activeIndex
    if (active < 0 || active >= ctx.optionCount) {
      listboxStore.activate(Math.max(0, Math.min(ctx.optionCount - 1, active)))
    }
  },
  { immediate: true },
)

function closePopover() {
  popover.close()
}

function focusSearchInput() {
  searchInputRef.value?.focus({ preventScroll: true })
}

function focusListbox() {
  listRef.value?.focus({ preventScroll: true })
}

function syncActiveOption(preferSelected = true) {
  if (!popover.state.value.open) return
  const selectedIndex = preferSelected
    ? filteredSequences.value.findIndex((sequence) => sequence.id === props.modelValue)
    : -1
  if (selectedIndex >= 0) {
    listboxStore.activate(selectedIndex)
    return
  }
  if (filteredSequences.value.length) {
    listboxStore.activate(0)
    return
  }
  listboxStore.clearSelection({ preserveActiveIndex: false })
}

watch(
  () => popover.state.value.open,
  async (open) => {
    if (!open) {
      searchQuery.value = ""
      return
    }
    syncActiveOption(true)
    await nextTick()
    focusSearchInput()
    await floating.updatePosition()
  },
)

watch(() => props.modelValue, () => {
  if (popover.state.value.open) {
    syncActiveOption(true)
  }
})

watch(searchQuery, () => {
  if (!popover.state.value.open) return
  nextTick(() => syncActiveOption(false))
})

const optionRefs = new Map<number, HTMLButtonElement>()

watch(activeIndex, (index) => {
  if (index < 0) return
  nextTick(() => {
    optionRefs.get(index)?.scrollIntoView({ block: "nearest" })
  })
})

function setOptionRef(index: number, el: Element | ComponentPublicInstance | null) {
  const raw = el
    ? (el instanceof Element ? el : (el.$el as Element | null))
    : null
  const button = raw instanceof HTMLButtonElement ? raw : null
  if (!button) {
    optionRefs.delete(index)
    return
  }
  optionRefs.set(index, button)
}

function optionId(index: number): string {
  return `${componentId}-option-${index}`
}

const activeOptionId = computed(() => {
  const index = activeIndex.value
  return index >= 0 ? optionId(index) : undefined
})

function emitSelection(value: number | null) {
  emit("update:modelValue", value)
  emit("change", value)
}

function commitSelectionAt(index: number, close = true) {
  const sequence = filteredSequences.value[index]
  if (!sequence) return
  emitSelection(sequence.id)
  if (close) {
    closePopover()
  }
}

function commitActiveSelection() {
  const index = activeIndex.value
  if (index < 0) return
  commitSelectionAt(index)
}

function moveActive(delta: number) {
  if (!filteredSequences.value.length) return
  listboxStore.move(delta, { loop: true })
}

function handleListKeydown(event: KeyboardEvent) {
  if (props.disabled) return
  switch (event.key) {
    case "ArrowDown":
      event.preventDefault()
      moveActive(1)
      return
    case "ArrowUp":
      event.preventDefault()
      moveActive(-1)
      return
    case "Home":
      event.preventDefault()
      if (filteredSequences.value.length) {
        listboxStore.activate(0)
      }
      return
    case "End":
      event.preventDefault()
      if (filteredSequences.value.length) {
        listboxStore.activate(filteredSequences.value.length - 1)
      }
      return
    case "Enter":
    case " ":
      event.preventDefault()
      commitActiveSelection()
      return
    case "Escape":
      event.preventDefault()
      closePopover()
      return
    default:
      return
  }
}

function handleSearchKeydown(event: KeyboardEvent) {
  if (props.disabled) return
  switch (event.key) {
    case "ArrowDown":
      event.preventDefault()
      if (!popover.state.value.open) {
        popover.open()
      }
      moveActive(1)
      focusListbox()
      return
    case "ArrowUp":
      event.preventDefault()
      moveActive(-1)
      focusListbox()
      return
    case "Enter":
      event.preventDefault()
      commitActiveSelection()
      return
    case "Escape":
      event.preventDefault()
      closePopover()
      return
    default:
      return
  }
}

const triggerBindings = computed(() => {
  const triggerBaseProps = popover.getTriggerProps({ type: "button", disabled: props.disabled })
  const { onClick, onKeydown, ...rest } = triggerBaseProps
  return {
    attrs: rest,
    onClick,
    onKeydown,
  }
})

const triggerProps = computed(() => ({
  ...triggerBindings.value.attrs,
  id: triggerId.value,
}))

const contentProps = computed(() => popover.getContentProps({ role: "dialog", tabIndex: -1 }))
const teleportTarget = computed(() => floating.teleportTarget.value)
const contentStyle = computed(() => floating.contentStyle.value)

function setFloatingContentRef(el: Element | ComponentPublicInstance | null) {
  const element = el
    ? (el instanceof Element ? el : (el.$el as Element | null))
    : null
  if (floating.contentRef) {
    floating.contentRef.value = element instanceof HTMLElement ? element : null
  }
}

function handleTriggerClick(event: MouseEvent) {
  triggerBindings.value.onClick?.(event)
}

function handleTriggerKeydown(event: KeyboardEvent) {
  triggerBindings.value.onKeydown?.(event)
  if (props.disabled) return
  switch (event.key) {
    case "ArrowDown":
    case "Enter":
    case " ":
      event.preventDefault()
      popover.open()
      return
    default:
      return
  }
}

function handleOptionClick(index: number) {
  listboxStore.activate(index)
  commitSelectionAt(index)
}

function handleOptionMouseEnter(index: number) {
  if (props.disabled) return
  listboxStore.activate(index)
}

function handleOptionPointerDown(event: PointerEvent) {
  event.preventDefault()
}

function handlePanelKeydown(event: KeyboardEvent) {
  if (event.key !== "Escape") return
  event.preventDefault()
  event.stopPropagation()
  closePopover()
}

function handleClear() {
  emitSelection(null)
  listboxStore.clearSelection({ preserveActiveIndex: false })
}

const hiddenInputValue = computed(() => (
  props.modelValue === null || typeof props.modelValue === "undefined"
    ? ""
    : String(props.modelValue)
))
</script>

<template>
  <div
    class="sequence-picker-combobox"
    :class="{ 'is-disabled': props.disabled, 'is-dark': isDarkTheme }"
  >
    <button
      :ref="floating.triggerRef"
      class="sequence-picker-combobox__trigger"
      :class="{ 'has-value': selectedSequence }"
      :disabled="props.disabled"
      v-bind="triggerProps"
      role="combobox"
      :aria-controls="popover.state.value.open ? listboxId : undefined"
      aria-haspopup="listbox"
      :aria-expanded="popover.state.value.open ? 'true' : 'false'"
      @click="handleTriggerClick"
      @keydown="handleTriggerKeydown"
    >
      <div class="sequence-picker-combobox__label-group">
        <span class="sequence-picker-combobox__label" :class="{ 'is-placeholder': !selectedSequence }">
          {{ selectedSequence?.label ?? props.placeholder }}
        </span>
        <span v-if="selectedSequence?.description" class="sequence-picker-combobox__meta">
          {{ selectedSequence.description }}
        </span>
      </div>
      <div class="sequence-picker-combobox__actions">
        <button
          v-if="props.clearable && !props.disabled && props.modelValue !== null"
          type="button"
          class="sequence-picker-combobox__clear"
          @click.stop="handleClear"
        >
          x
        </button>
        <span class="sequence-picker-combobox__chevron" aria-hidden="true">v</span>
      </div>
    </button>

    <input v-if="props.name" :name="props.name" type="hidden" autocomplete="off" :value="hiddenInputValue">

    <Teleport v-if="popover.state.value.open && teleportTarget" :to="teleportTarget">
      <div
        :ref="setFloatingContentRef"
        class="sequence-picker-combobox__popover"
        :class="{ 'is-dark': isDarkTheme }"
        :style="contentStyle"
        v-bind="contentProps"
        :aria-labelledby="triggerId"
        @keydown.capture="handlePanelKeydown"
      >
        <div class="sequence-picker-combobox__controls">
          <input
            ref="searchInputRef"
            v-model="searchQuery"
            type="search"
            class="sequence-picker-combobox__search"
            placeholder="Search instructions"
            spellcheck="false"
            autocomplete="off"
            :disabled="props.disabled"
            @keydown="handleSearchKeydown"
          >
        </div>

        <div
          ref="listRef"
          class="sequence-picker-combobox__list"
          role="listbox"
          :aria-activedescendant="activeOptionId"
          :id="listboxId"
          :tabindex="filteredSequences.length ? 0 : -1"
          @keydown="handleListKeydown"
        >
          <button
            v-for="(sequence, index) in filteredSequences"
            :id="optionId(index)"
            :key="sequence.id"
            type="button"
            role="option"
            class="sequence-picker-combobox__option"
            :class="{
              'is-active': activeIndex === index,
              'is-selected': sequence.id === props.modelValue,
            }"
            :aria-selected="sequence.id === props.modelValue ? 'true' : 'false'"
            @pointerdown="handleOptionPointerDown"
            @mouseenter="handleOptionMouseEnter(index)"
            @click="handleOptionClick(index)"
            :ref="(el) => setOptionRef(index, el)"
          >
            <span class="sequence-picker-combobox__option-body">
              <span class="sequence-picker-combobox__option-row">
                <span class="sequence-picker-combobox__option-label">{{ sequence.label }}</span>
                <span v-if="sequence.system" class="sequence-picker-combobox__badge">
                  System
                </span>
                <span v-if="sequence.readOnly" class="sequence-picker-combobox__badge is-muted">
                  Read-only
                </span>
              </span>
              <span v-if="sequence.description" class="sequence-picker-combobox__option-description">
                {{ sequence.description }}
              </span>
            </span>
            <span
              v-if="sequence.id === props.modelValue"
              class="sequence-picker-combobox__selected-mark"
              aria-hidden="true"
            >
              ✓
            </span>
          </button>

          <div v-if="!filteredSequences.length" class="sequence-picker-combobox__empty">
            {{ emptyStateLabel }}
          </div>
        </div>
      </div>
    </Teleport>
  </div>
</template>

<style scoped>
.sequence-picker-combobox {
  position: relative;
  width: 100%;
}

.sequence-picker-combobox,
.sequence-picker-combobox__popover {
  --picker-surface: #ffffff;
  --picker-surface-muted: #f8fafc;
  --picker-border: rgba(15, 23, 42, 0.15);
  --picker-border-hover: rgba(37, 99, 235, 0.65);
  --picker-text: #0f172a;
  --picker-muted: #4b5563;
  --picker-placeholder: #6b7280;
  --picker-accent-soft: rgba(37, 99, 235, 0.08);
  --picker-outline: rgba(37, 99, 235, 0.35);
  --picker-shadow: 0 18px 45px rgba(15, 23, 42, 0.14);
  --picker-trigger-shadow: 0 1px 2px rgba(15, 23, 42, 0.06);
  --picker-option-hover: rgba(15, 23, 42, 0.04);
  --picker-divider: rgba(15, 23, 42, 0.06);
  --picker-input-bg: #ffffff;
  --picker-input-border: rgba(15, 23, 42, 0.15);
  --picker-input-text: #0f172a;
  --picker-input-placeholder: #6b7280;
  --picker-badge-bg: rgba(15, 23, 42, 0.06);
  --picker-badge-text: #334155;
}

.sequence-picker-combobox.is-dark,
.sequence-picker-combobox__popover.is-dark,
:global(.dark .sequence-picker-combobox),
:global(.dark .sequence-picker-combobox__popover){
  --picker-surface: rgba(9, 12, 20, 0.98);
  --picker-surface-muted: rgba(20, 26, 38, 0.92);
  --picker-border: rgba(148, 163, 184, 0.38);
  --picker-border-hover: rgba(129, 140, 248, 0.85);
  --picker-text: #f3f4f6;
  --picker-muted: #a5b4cf;
  --picker-placeholder: #94a3b8;
  --picker-accent-soft: rgba(147, 197, 253, 0.18);
  --picker-outline: rgba(147, 197, 253, 0.65);
  --picker-shadow: 0 28px 60px rgba(2, 6, 23, 0.75);
  --picker-trigger-shadow: 0 1px 2px rgba(0, 0, 0, 0.55);
  --picker-option-hover: rgba(255, 255, 255, 0.04);
  --picker-divider: rgba(255, 255, 255, 0.09);
  --picker-input-bg: rgba(15, 23, 42, 0.92);
  --picker-input-border: rgba(148, 163, 184, 0.45);
  --picker-input-text: #f1f5f9;
  --picker-input-placeholder: #94a3b8;
  --picker-badge-bg: rgba(255, 255, 255, 0.08);
  --picker-badge-text: #cbd5e1;
}

.sequence-picker-combobox__trigger {
  width: 100%;
  border-radius: 0.45rem;
  border: 1px solid var(--picker-border);
  background: var(--picker-surface);
  padding: 0.65rem 0.85rem;
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 0.75rem;
  text-align: left;
  font-size: 0.85rem;
  line-height: 1.2;
  color: var(--picker-text);
  transition: border-color 150ms ease, box-shadow 150ms ease, background 150ms ease;
  box-shadow: var(--picker-trigger-shadow);
}

.sequence-picker-combobox__trigger:hover {
  border-color: var(--picker-border-hover);
}

.sequence-picker-combobox__trigger:focus-visible {
  outline: 2px solid var(--picker-outline);
  outline-offset: 2px;
}

.sequence-picker-combobox__trigger:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.sequence-picker-combobox__label-group {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.sequence-picker-combobox__label {
  font-weight: 600;
  color: var(--picker-text);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.sequence-picker-combobox__label.is-placeholder {
  font-weight: 500;
  color: var(--picker-placeholder);
}

.sequence-picker-combobox__meta {
  font-size: 0.75rem;
  color: var(--picker-muted);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.sequence-picker-combobox__actions {
  display: flex;
  align-items: center;
  gap: 0.25rem;
}

.sequence-picker-combobox__clear {
  border: none;
  background: transparent;
  font-size: 0.85rem;
  color: var(--picker-muted);
  padding: 0.1rem;
  border-radius: 999px;
  cursor: pointer;
  transition: color 120ms ease, background 120ms ease;
}

.sequence-picker-combobox__clear:hover {
  color: var(--picker-text);
  background: var(--picker-option-hover);
}

.sequence-picker-combobox__chevron {
  color: var(--picker-muted);
  font-size: 0.85rem;
}

.sequence-picker-combobox__popover {
  width: 360px;
  max-height: min(360px, 70vh);
  border-radius: 0.65rem;
  border: 1px solid var(--picker-border);
  background: var(--picker-surface);
  box-shadow: var(--picker-shadow);
  padding: 0.75rem;
  display: flex;
  flex-direction: column;
  gap: 0.55rem;
}

.sequence-picker-combobox__controls {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  padding-bottom: 0.5rem;
  border-bottom: 1px solid var(--picker-divider);
}

.sequence-picker-combobox__search {
  flex: 1;
  border-radius: 0.65rem;
  border: 1px solid var(--picker-input-border);
  padding: 0.5rem 0.9rem;
  font-size: 0.85rem;
  line-height: 1.3;
  background: var(--picker-input-bg);
  color: var(--picker-input-text);
  transition: border-color 150ms ease, background 150ms ease, box-shadow 150ms ease;
}

.sequence-picker-combobox__search::placeholder {
  color: var(--picker-input-placeholder);
}

.sequence-picker-combobox__search:focus-visible {
  border-color: var(--picker-border-hover);
  outline: none;
  background: var(--picker-surface);
  box-shadow: 0 0 0 1px var(--picker-accent-soft);
}

.sequence-picker-combobox__list {
  display: flex;
  flex-direction: column;
  gap: 0.35rem;
  overflow: auto;
  min-height: 0;
  outline: none;
}

.sequence-picker-combobox__option {
  width: 100%;
  border: none;
  background: transparent;
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 0.75rem;
  padding: 0.65rem 0.7rem;
  border-radius: 0.6rem;
  color: var(--picker-text);
  text-align: left;
  transition: background 140ms ease, color 140ms ease;
}

.sequence-picker-combobox__option:hover,
.sequence-picker-combobox__option.is-active {
  background: var(--picker-option-hover);
}

.sequence-picker-combobox__option.is-selected {
  background: var(--picker-accent-soft);
}

.sequence-picker-combobox__option-body {
  display: flex;
  flex-direction: column;
  gap: 0.2rem;
  min-width: 0;
  flex: 1;
}

.sequence-picker-combobox__option-row {
  display: flex;
  align-items: center;
  gap: 0.35rem;
  flex-wrap: wrap;
}

.sequence-picker-combobox__option-label {
  font-weight: 600;
}

.sequence-picker-combobox__option-description {
  font-size: 0.75rem;
  line-height: 1.35;
  color: var(--picker-muted);
}

.sequence-picker-combobox__badge {
  display: inline-flex;
  align-items: center;
  border-radius: 999px;
  padding: 0.12rem 0.45rem;
  font-size: 0.68rem;
  line-height: 1.1;
  background: var(--picker-badge-bg);
  color: var(--picker-badge-text);
}

.sequence-picker-combobox__badge.is-muted {
  opacity: 0.9;
}

.sequence-picker-combobox__selected-mark {
  padding-top: 0.05rem;
  font-size: 0.85rem;
}

.sequence-picker-combobox__empty {
  padding: 0.8rem 0.5rem;
  text-align: center;
  font-size: 0.8rem;
  color: var(--picker-muted);
}
</style>
