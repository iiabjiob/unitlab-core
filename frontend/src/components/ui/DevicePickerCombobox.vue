<script setup lang="ts">
import { computed, nextTick, ref, watch } from "vue"
import type { ComponentPublicInstance } from "vue"
import { usePopoverController, useFloatingPopover } from "@affino/popover-vue"
import { createListboxStore, useListboxStore } from "@affino/listbox-vue"

import { useDeviceStore } from "@/stores/deviceStore"
import { useThemeStore } from "@/stores/themeStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"
import type { Device, DeviceStatus, DeviceType } from "@/types/device"
import { APP_OVERLAY_HOST_SELECTOR } from "@/utils/overlayHost"

const props = withDefaults(defineProps<{
  modelValue: number | null
  devices?: Device[]
  placeholder?: string
  disabled?: boolean
  allowedTypes?: DeviceType[]
  onlineOnly?: boolean
  clearable?: boolean
  id?: string
  name?: string
}>(), {
  devices: undefined,
  placeholder: "— select device —",
  disabled: false,
  allowedTypes: undefined,
  onlineOnly: true,
  clearable: true,
  id: undefined,
  name: undefined,
})

const emit = defineEmits<{
  (e: "update:modelValue", value: number | null): void
  (e: "change", value: number | null): void
}>()

const deviceStore = useDeviceStore()
const themeStore = useThemeStore()
void runStoreBootstrap(
  ["device-picker-combobox"],
  [() => deviceStore.ensureLoaded()],
  { mode: "settled" },
)

const searchQuery = ref("")
const showOnlineOnly = ref(Boolean(props.onlineOnly))
const searchInputRef = ref<HTMLInputElement | null>(null)
const listRef = ref<HTMLDivElement | null>(null)
const isDarkTheme = computed(() => themeStore.currentTheme === "dark")

const allowedTypeSet = computed(() => {
  const raw = props.allowedTypes?.map(type => type?.toLowerCase().trim()).filter(Boolean) ?? []
  return raw.length ? new Set(raw) : null
})

const sourceDevices = computed(() => props.devices ?? deviceStore.devices)

interface NormalizedDevice {
  id: number
  label: string
  unitId: string
  status: DeviceStatus
  searchable: string
}

const normalizedDevices = computed<NormalizedDevice[]>(() => {
  return (sourceDevices.value ?? [])
    .filter((device) => {
      const type = device.device_type?.toLowerCase?.() ?? ""
      if (allowedTypeSet.value && !allowedTypeSet.value.has(type)) {
        return false
      }
      return true
    })
    .map((device) => ({
      id: device.id,
      label: device.display_name ?? device.unit_id,
      unitId: device.unit_id,
      status: device.status,
      searchable: `${(device.display_name ?? "").toLowerCase()} ${device.unit_id.toLowerCase()}`,
    }))
    .sort((a, b) => {
      if (a.status !== b.status) {
        return a.status === "online" ? -1 : 1
      }
      return a.label.localeCompare(b.label)
    })
})

const selectedDevice = computed(() => normalizedDevices.value.find(device => device.id === props.modelValue) ?? null)

const filteredDevices = computed(() => {
  const query = searchQuery.value.trim().toLowerCase()
  const onlineOnly = showOnlineOnly.value
  const matches: NormalizedDevice[] = []

  normalizedDevices.value.forEach((device) => {
    if (onlineOnly && device.status !== "online") {
      return
    }
    if (query.length && !device.searchable.includes(query)) {
      return
    }
    matches.push(device)
  })

  if (selectedDevice.value && !matches.some(device => device.id === selectedDevice.value?.id)) {
    matches.unshift(selectedDevice.value)
  }

  return matches
})

const hasDevicesLoaded = computed(() => normalizedDevices.value.length > 0)

const emptyStateLabel = computed(() => {
  if (!hasDevicesLoaded.value) {
    return "No devices available"
  }
  if (searchQuery.value.trim().length > 0) {
    return "No devices match your search"
  }
  if (showOnlineOnly.value) {
    return "No online devices available"
  }
  return "No devices available"
})

const componentId = `device-picker-${Math.random().toString(36).slice(2, 10)}`
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

const listboxContext = computed(() => ({
  optionCount: filteredDevices.value.length,
  isDisabled: (index: number) => !filteredDevices.value[index],
}))

watch(listboxContext, (ctx) => {
  listboxStore.setContext(ctx)
  if (ctx.optionCount <= 0) {
    listboxStore.clearSelection({ preserveActiveIndex: false })
    return
  }
  const active = listboxStore.peekState().activeIndex
  if (active < 0 || active >= ctx.optionCount) {
    listboxStore.activate(Math.max(0, Math.min(ctx.optionCount - 1, active)))
  }
}, { immediate: true })

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
    ? filteredDevices.value.findIndex(device => device.id === props.modelValue)
    : -1
  if (selectedIndex >= 0) {
    listboxStore.activate(selectedIndex)
    return
  }
  if (filteredDevices.value.length) {
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
      showOnlineOnly.value = Boolean(props.onlineOnly)
      return
    }
    syncActiveOption(true)
    await nextTick()
    focusSearchInput()
    await floating.updatePosition()
  },
)

watch(() => props.onlineOnly, (next) => {
  showOnlineOnly.value = Boolean(next)
  if (popover.state.value.open) {
    syncActiveOption(true)
  }
})

watch(() => props.modelValue, () => {
  if (popover.state.value.open) {
    syncActiveOption(true)
  }
})

watch([searchQuery, showOnlineOnly], () => {
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
  const device = filteredDevices.value[index]
  if (!device) return
  emitSelection(device.id)
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
  if (!filteredDevices.value.length) return
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
      if (filteredDevices.value.length) {
        listboxStore.activate(0)
      }
      return
    case "End":
      event.preventDefault()
      if (filteredDevices.value.length) {
        listboxStore.activate(filteredDevices.value.length - 1)
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
  // Keep focus on the popover content while clicking options
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
</script>

<template>
  <div
    class="device-picker-combobox"
    :class="{ 'is-disabled': props.disabled, 'is-dark': isDarkTheme }"
  >
    <button
      :ref="floating.triggerRef"
      class="device-picker-combobox__trigger !dark:bg-neutral-900 !dark:text-neutral-100"
      :class="{ 'has-value': selectedDevice }"
      :disabled="props.disabled"
      v-bind="triggerProps"
      role="combobox"
      :aria-controls="popover.state.value.open ? listboxId : undefined"
      aria-haspopup="listbox"
      :aria-expanded="popover.state.value.open ? 'true' : 'false'"
      @click="handleTriggerClick"
      @keydown="handleTriggerKeydown"
    >
      <div class="device-picker-combobox__label-group">
        <span class="device-picker-combobox__label" :class="{ 'is-placeholder': !selectedDevice }">
          {{ selectedDevice?.label ?? props.placeholder }}
        </span>
        <span v-if="selectedDevice" class="device-picker-combobox__meta">
          {{ selectedDevice.unitId }} · {{ selectedDevice.status === "online" ? "Online" : "Offline" }}
        </span>
      </div>
      <div class="device-picker-combobox__actions">
        <button
          v-if="props.clearable && !props.disabled && props.modelValue !== null"
          type="button"
          class="device-picker-combobox__clear"
          @click.stop="handleClear"
        >
          ✕
        </button>
        <span class="device-picker-combobox__chevron" aria-hidden="true">▾</span>
      </div>
    </button>

    <input v-if="props.name" :name="props.name" type="hidden" autocomplete="off" :value="hiddenInputValue">

    <Teleport v-if="popover.state.value.open && teleportTarget" :to="teleportTarget">
      <div
        :ref="setFloatingContentRef"
        class="device-picker-combobox__popover"
        :class="{ 'is-dark': isDarkTheme }"
        :style="contentStyle"
        v-bind="contentProps"
        :aria-labelledby="triggerId"
        @keydown.capture="handlePanelKeydown"
      >
        <div class="device-picker-combobox__controls">
          <input
            ref="searchInputRef"
            v-model="searchQuery"
            type="search"
            class="device-picker-combobox__search"
            placeholder="Search devices"
            spellcheck="false"
            autocomplete="off"
            :disabled="props.disabled"
            @keydown="handleSearchKeydown"
          >
          <label class="device-picker-combobox__filter-checkbox">
            <input
              v-model="showOnlineOnly"
              type="checkbox"
              autocomplete="off"
              name="device-picker-online-only"
              :disabled="props.disabled"
            >
            <span>Online only</span>
          </label>
        </div>

        <div
          ref="listRef"
          class="device-picker-combobox__list"
          role="listbox"
          :aria-activedescendant="activeOptionId"
          :id="listboxId"
          :tabindex="filteredDevices.length ? 0 : -1"
          @keydown="handleListKeydown"
        >
          <button
            v-for="(device, index) in filteredDevices"
            :id="optionId(index)"
            :key="device.id"
            type="button"
            role="option"
            class="device-picker-combobox__option"
            :class="{
              'is-active': activeIndex === index,
              'is-selected': device.id === props.modelValue,
              'is-offline': device.status !== 'online',
            }"
            :aria-selected="device.id === props.modelValue ? 'true' : 'false'"
            @pointerdown="handleOptionPointerDown"
            @mouseenter="handleOptionMouseEnter(index)"
            @click="handleOptionClick(index)"
            :ref="(el) => setOptionRef(index, el)"
          >
            <span
              class="device-picker-combobox__status-dot"
              :class="device.status === 'online' ? 'is-online' : 'is-offline'"
              aria-hidden="true"
            ></span>
            <span class="device-picker-combobox__option-body">
              <span class="device-picker-combobox__option-label">{{ device.label }}</span>
              <span
                v-if="device.unitId && device.unitId !== device.label"
                class="device-picker-combobox__option-unit"
              >
                · {{ device.unitId }}
              </span>
            </span>
            <span
              v-if="device.id === props.modelValue"
              class="device-picker-combobox__selected-mark"
              aria-hidden="true"
            >
              ✓
            </span>
          </button>

          <div v-if="!filteredDevices.length" class="device-picker-combobox__empty">
            {{ emptyStateLabel }}
          </div>
        </div>
      </div>
    </Teleport>
  </div>
</template>

<style scoped>
.device-picker-combobox {
  position: relative;
  width: 100%;
}

.device-picker-combobox,
.device-picker-combobox__popover {
  --picker-surface: #ffffff;
  --picker-surface-muted: #f8fafc;
  --picker-border: rgba(15, 23, 42, 0.15);
  --picker-border-hover: rgba(37, 99, 235, 0.65);
  --picker-text: #0f172a;
  --picker-muted: #4b5563;
  --picker-placeholder: #6b7280;
  --picker-accent: #2563eb;
  --picker-accent-soft: rgba(37, 99, 235, 0.08);
  --picker-outline: rgba(37, 99, 235, 0.35);
  --picker-shadow: 0 18px 45px rgba(15, 23, 42, 0.14);
  --picker-trigger-shadow: 0 1px 2px rgba(15, 23, 42, 0.06);
  --picker-option-border: rgba(15, 23, 42, 0.08);
  --picker-option-hover: rgba(15, 23, 42, 0.04);
  --picker-divider: rgba(15, 23, 42, 0.06);
  --picker-status-online-bg: rgba(34, 197, 94, 0.15);
  --picker-status-online-text: #15803d;
  --picker-status-offline-bg: rgba(248, 113, 113, 0.2);
  --picker-status-offline-text: #b91c1c;
  --picker-input-bg: #ffffff;
  --picker-input-border: rgba(15, 23, 42, 0.15);
  --picker-input-text: #0f172a;
  --picker-input-placeholder: #6b7280;
}

.device-picker-combobox.is-dark,
.device-picker-combobox__popover.is-dark,
:global(.dark) .device-picker-combobox,
:global(.dark) .device-picker-combobox__popover {
  --picker-surface: rgba(9, 12, 20, 0.98);
  --picker-surface-muted: rgba(20, 26, 38, 0.92);
  --picker-border: rgba(148, 163, 184, 0.38);
  --picker-border-hover: rgba(129, 140, 248, 0.85);
  --picker-text: #f3f4f6;
  --picker-muted: #a5b4cf;
  --picker-placeholder: #94a3b8;
  --picker-accent: #93c5fd;
  --picker-accent-soft: rgba(147, 197, 253, 0.18);
  --picker-outline: rgba(147, 197, 253, 0.65);
  --picker-shadow: 0 28px 60px rgba(2, 6, 23, 0.75);
  --picker-trigger-shadow: 0 1px 2px rgba(0, 0, 0, 0.55);
  --picker-option-border: rgba(255, 255, 255, 0.08);
  --picker-option-hover: rgba(255, 255, 255, 0.04);
  --picker-divider: rgba(255, 255, 255, 0.09);
  --picker-status-online-bg: rgba(34, 197, 94, 0.2);
  --picker-status-online-text: #4ade80;
  --picker-status-offline-bg: rgba(248, 113, 113, 0.2);
  --picker-status-offline-text: #fca5a5;
  --picker-input-bg: rgba(15, 23, 42, 0.92);
  --picker-input-border: rgba(148, 163, 184, 0.45);
  --picker-input-text: #f1f5f9;
  --picker-input-placeholder: #94a3b8;
}

.device-picker-combobox__trigger {
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

.device-picker-combobox__trigger:hover {
  border-color: var(--picker-border-hover);
}

.device-picker-combobox__trigger:focus-visible {
  outline: 2px solid var(--picker-outline);
  outline-offset: 2px;
}

.device-picker-combobox__trigger:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.device-picker-combobox__label-group {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.device-picker-combobox__label {
  font-weight: 600;
  color: var(--picker-text);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.device-picker-combobox__label.is-placeholder {
  font-weight: 500;
  color: var(--picker-placeholder);
}

.device-picker-combobox__meta {
  font-size: 0.75rem;
  color: var(--picker-muted);
}

.device-picker-combobox__actions {
  display: flex;
  align-items: center;
  gap: 0.25rem;
}

.device-picker-combobox__clear {
  border: none;
  background: transparent;
  font-size: 0.85rem;
  color: var(--picker-muted);
  padding: 0.1rem;
  border-radius: 999px;
  cursor: pointer;
  transition: color 120ms ease, background 120ms ease;
}

.device-picker-combobox__clear:hover {
  color: var(--picker-text);
  background: var(--picker-option-hover);
}

.device-picker-combobox__chevron {
  color: var(--picker-muted);
  font-size: 0.85rem;
}

.device-picker-combobox__popover {
  width: 320px;
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

.device-picker-combobox__controls {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  flex-wrap: wrap;
  padding-bottom: 0.5rem;
  border-bottom: 1px solid var(--picker-divider);
}

.device-picker-combobox__search {
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

.device-picker-combobox__search::placeholder {
  color: var(--picker-input-placeholder);
}

.device-picker-combobox__search:focus-visible {
  border-color: var(--picker-border-hover);
  outline: none;
  background: var(--picker-surface);
  box-shadow: 0 0 0 1px var(--picker-accent-soft);
}

.device-picker-combobox__search:disabled {
  opacity: 0.55;
}

.device-picker-combobox__filter-checkbox {
  display: inline-flex;
  align-items: center;
  gap: 0.4rem;
  font-size: 0.75rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--picker-muted);
  font-weight: 600;
  white-space: nowrap;
  cursor: pointer;
}

.device-picker-combobox__filter-checkbox input {
  width: 1rem;
  height: 1rem;
  border-radius: 0.25rem;
  border: 1px solid var(--picker-border);
  background: var(--picker-surface);
  accent-color: var(--picker-accent);
}

.device-picker-combobox__filter-checkbox input:disabled + span {
  opacity: 0.5;
}

.device-picker-combobox__list {
  display: flex;
  flex-direction: column;
  gap: 0.35rem;
  max-height: 240px;
  overflow-y: auto;
  outline: none;
  padding: 0.2rem 0;
}

.device-picker-combobox__option {
  border-radius: 0.55rem;
  border: 1px solid transparent;
  padding: 0.55rem 0.65rem;
  display: flex;
  align-items: center;
  gap: 0.75rem;
  text-align: left;
  background: var(--picker-surface-muted);
  color: var(--picker-text);
  cursor: pointer;
  transition: border-color 140ms ease, background 140ms ease, color 140ms ease;
  min-height: 2.5rem;
}

.device-picker-combobox__option:hover {
  background: var(--picker-option-hover);
  border-color: var(--picker-option-border);
}

.device-picker-combobox__option.is-active {
  border-color: var(--picker-accent);
  background: var(--picker-accent-soft);
}

.device-picker-combobox__option.is-selected {
  border-color: var(--picker-accent);
  background: var(--picker-accent-soft);
}

.device-picker-combobox__option.is-offline {
  opacity: 0.85;
}

.device-picker-combobox__status-dot {
  width: 0.55rem;
  height: 0.55rem;
  border-radius: 999px;
  flex-shrink: 0;
  border: 2px solid transparent;
}

.device-picker-combobox__status-dot.is-online {
  background: var(--picker-status-online-bg);
  border-color: var(--picker-status-online-text);
}

.device-picker-combobox__status-dot.is-offline {
  background: var(--picker-status-offline-bg);
  border-color: var(--picker-status-offline-text);
}

.device-picker-combobox__option-body {
  display: flex;
  align-items: center;
  gap: 0.35rem;
  min-width: 0;
  flex: 1;
}

.device-picker-combobox__option-label {
  font-weight: 600;
  color: var(--picker-text);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.device-picker-combobox__option-unit {
  font-size: 0.78rem;
  color: var(--picker-muted);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.device-picker-combobox__selected-mark {
  font-size: 0.9rem;
  color: var(--picker-accent);
  margin-left: auto;
}

.device-picker-combobox__empty {
  font-size: 0.85rem;
  color: var(--picker-muted);
  text-align: center;
  padding: 1rem 0.5rem;
}
</style>