<script setup lang="ts">
import { computed, nextTick, ref, watch } from "vue"
import type { ComponentPublicInstance } from "vue"
import { usePopoverController, useFloatingPopover } from "@affino/popover-vue"
import { createListboxStore, useListboxStore } from "@affino/listbox-vue"

import { useDeviceStore } from "@/stores/deviceStore"
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
void runStoreBootstrap(
  ["device-picker-combobox"],
  [() => deviceStore.ensureLoaded()],
  { mode: "settled" },
)

const searchQuery = ref("")
const showOnlineOnly = ref(Boolean(props.onlineOnly))
const searchInputRef = ref<HTMLInputElement | null>(null)
const listRef = ref<HTMLDivElement | null>(null)

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
    :class="{ 'is-disabled': props.disabled }"
  >
    <button
      :ref="floating.triggerRef"
      class="device-picker-combobox__trigger"
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
            @keydown="handleSearchKeydown"
          >
          <button
            type="button"
            class="device-picker-combobox__filter"
            :class="{ 'is-active': showOnlineOnly }"
            @click="showOnlineOnly = !showOnlineOnly"
          >
            Online only
          </button>
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
            <div class="device-picker-combobox__option-text">
              <span class="device-picker-combobox__option-label">{{ device.label }}</span>
              <span class="device-picker-combobox__option-meta">{{ device.unitId }}</span>
            </div>
            <div class="device-picker-combobox__option-info">
              <span class="device-picker-combobox__status" :class="device.status === 'online' ? 'is-online' : 'is-offline'">
                {{ device.status === "online" ? "Online" : "Offline" }}
              </span>
              <span
                v-if="device.id === props.modelValue"
                class="device-picker-combobox__selected-mark"
                aria-hidden="true"
              >
                ✓
              </span>
            </div>
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

.device-picker-combobox__trigger {
  width: 100%;
  border-radius: 0.5rem;
  border: 1px solid hsl(0 0% 82%);
  background: var(--device-picker-trigger-bg, #fff);
  padding: 0.65rem 0.85rem;
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 0.75rem;
  text-align: left;
  font-size: 0.85rem;
  line-height: 1.2;
  color: #1f2937;
  transition: border-color 120ms ease, box-shadow 120ms ease;
}

.device-picker-combobox__trigger:hover {
  border-color: hsl(222 47% 55%);
}

.device-picker-combobox__trigger:focus-visible {
  outline: 2px solid hsl(222 90% 60% / 0.35);
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
  color: inherit;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.device-picker-combobox__label.is-placeholder {
  font-weight: 500;
  color: #6b7280;
}

.device-picker-combobox__meta {
  font-size: 0.75rem;
  color: #6b7280;
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
  color: #9ca3af;
  padding: 0.1rem;
  border-radius: 999px;
  cursor: pointer;
  transition: color 100ms ease, background 100ms ease;
}

.device-picker-combobox__clear:hover {
  color: #111827;
  background: rgba(15, 23, 42, 0.08);
}

.device-picker-combobox__chevron {
  color: #9ca3af;
  font-size: 0.85rem;
}

.device-picker-combobox__popover {
  width: 320px;
  max-height: min(360px, 70vh);
  border-radius: 0.75rem;
  border: 1px solid hsl(0 0% 82%);
  background: #fff;
  box-shadow: 0 20px 45px rgba(15, 23, 42, 0.15);
  padding: 0.85rem;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

.device-picker-combobox__controls {
  display: flex;
  gap: 0.5rem;
}

.device-picker-combobox__search {
  flex: 1;
  border-radius: 999px;
  border: 1px solid hsl(214 11% 80%);
  padding: 0.45rem 0.85rem;
  font-size: 0.85rem;
}

.device-picker-combobox__filter {
  border-radius: 999px;
  border: 1px solid hsl(214 11% 80%);
  padding: 0.45rem 0.9rem;
  font-size: 0.8rem;
  background: #f9fafb;
  color: #6b7280;
  transition: all 120ms ease;
}

.device-picker-combobox__filter.is-active {
  background: hsl(222 85% 55% / 0.12);
  color: #1f2a4a;
  border-color: hsl(222 85% 55%);
}

.device-picker-combobox__list {
  display: flex;
  flex-direction: column;
  gap: 0.4rem;
  max-height: 240px;
  overflow-y: auto;
  outline: none;
}

.device-picker-combobox__option {
  border-radius: 0.65rem;
  border: 1px solid transparent;
  padding: 0.55rem 0.65rem;
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 0.75rem;
  text-align: left;
  background: #f9fafb;
  cursor: pointer;
  transition: border-color 120ms ease, background 120ms ease, color 120ms ease;
}

.device-picker-combobox__option:hover {
  border-color: hsl(222 47% 62%);
  background: #fff;
}

.device-picker-combobox__option.is-active {
  border-color: hsl(222 85% 60%);
  background: hsl(222 85% 60% / 0.08);
}

.device-picker-combobox__option.is-selected {
  border-color: hsl(221 83% 53%);
  background: hsl(221 83% 53% / 0.12);
}

.device-picker-combobox__option.is-offline {
  opacity: 0.78;
}

.device-picker-combobox__option-text {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.device-picker-combobox__option-label {
  font-weight: 600;
  color: #111827;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.device-picker-combobox__option-meta {
  font-size: 0.75rem;
  color: #6b7280;
}

.device-picker-combobox__option-info {
  display: flex;
  align-items: center;
  gap: 0.35rem;
}

.device-picker-combobox__status {
  font-size: 0.75rem;
  font-weight: 600;
  padding: 0.25rem 0.55rem;
  border-radius: 999px;
}

.device-picker-combobox__status.is-online {
  background: #dcfce7;
  color: #166534;
}

.device-picker-combobox__status.is-offline {
  background: #fee2e2;
  color: #9f1239;
}

.device-picker-combobox__selected-mark {
  font-size: 0.85rem;
  color: #2563eb;
}

.device-picker-combobox__empty {
  font-size: 0.85rem;
  color: #6b7280;
  text-align: center;
  padding: 1rem 0.5rem;
}

:global(.dark) .device-picker-combobox__trigger {
  border-color: rgba(148, 163, 184, 0.4);
  background: rgba(15, 23, 42, 0.75);
  color: #e5e7eb;
}

:global(.dark) .device-picker-combobox__label.is-placeholder {
  color: #94a3b8;
}

:global(.dark) .device-picker-combobox__meta {
  color: #94a3b8;
}

:global(.dark) .device-picker-combobox__popover {
  background: #0f172a;
  border-color: rgba(148, 163, 184, 0.3);
}

:global(.dark) .device-picker-combobox__search {
  border-color: rgba(148, 163, 184, 0.4);
  background: rgba(15, 23, 42, 0.85);
  color: #e2e8f0;
}

:global(.dark) .device-picker-combobox__filter {
  border-color: rgba(148, 163, 184, 0.4);
  background: rgba(15, 23, 42, 0.65);
  color: #cbd5f5;
}

:global(.dark) .device-picker-combobox__filter.is-active {
  background: rgba(99, 102, 241, 0.25);
  border-color: rgba(129, 140, 248, 0.9);
  color: #e0e7ff;
}

:global(.dark) .device-picker-combobox__list {
  background: transparent;
}

:global(.dark) .device-picker-combobox__option {
  background: rgba(15, 23, 42, 0.75);
  border-color: transparent;
}

:global(.dark) .device-picker-combobox__option:hover {
  background: rgba(30, 41, 59, 0.9);
  border-color: rgba(191, 219, 254, 0.4);
}

:global(.dark) .device-picker-combobox__option.is-active {
  background: rgba(59, 130, 246, 0.15);
  border-color: rgba(59, 130, 246, 0.6);
}

:global(.dark) .device-picker-combobox__option.is-selected {
  background: rgba(59, 130, 246, 0.22);
  border-color: rgba(59, 130, 246, 0.85);
}

:global(.dark) .device-picker-combobox__option-label {
  color: #e2e8f0;
}

:global(.dark) .device-picker-combobox__option-meta {
  color: #cbd5f5;
}

:global(.dark) .device-picker-combobox__status.is-online {
  background: rgba(34, 197, 94, 0.25);
  color: #4ade80;
}

:global(.dark) .device-picker-combobox__status.is-offline {
  background: rgba(248, 113, 113, 0.25);
  color: #fca5a5;
}

:global(.dark) .device-picker-combobox__selected-mark {
  color: #93c5fd;
}

:global(.dark) .device-picker-combobox__empty {
  color: #94a3b8;
}
</style>
