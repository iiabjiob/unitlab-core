<template>
  <div class="channel-tree-picker">
    <input
      v-if="name"
      type="hidden"
      autocomplete="off"
      :name="name"
      :value="modelValue ?? ''"
    />

    <button
      :ref="floating.triggerRef"
      type="button"
      class="channel-tree-picker__trigger"
      :disabled="disabled"
      v-bind="triggerProps"
      @click.stop
    >
      <span class="channel-tree-picker__value" :class="valueClass">{{ valueLabel }}</span>
      <span class="channel-tree-picker__chevron" aria-hidden="true">▾</span>
    </button>
    <Teleport v-if="popover.state.value.open && popoverTeleportTarget" :to="popoverTeleportTarget">
      <div
        :ref="floating.contentRef"
        class="channel-tree-picker__popover"
        :style="popoverContentStyle"
        v-bind="contentProps"
        @click.stop
        @keydown="onTreeRootKeydown"
      >
        <div class="channel-tree-picker__header">
          <span class="channel-tree-picker__title">Select channel</span>
          <button
            v-if="modelValue !== null"
            type="button"
            class="channel-tree-picker__clear"
            :disabled="disabled"
            @click="handleClear"
          >
            Clear
          </button>
        </div>

        <div v-if="visibleNodes.length === 0" class="channel-tree-picker__empty">
          {{ catalogLoading ? "Loading channels…" : "No channels available" }}
        </div>

        <div
          v-else
          class="channel-tree-picker__tree"
          role="tree"
          aria-label="Channel tree"
        >
          <button
            v-for="node in visibleNodes"
            :key="node.value"
            :ref="bindItemElement(node.value)"
            type="button"
            class="channel-tree-picker__node"
            :class="{
              'is-active': isNodeActive(node.value),
              'is-selected': isNodeSelected(node.value),
              'is-disabled': isNodeDisabled(node.value),
              'is-channel': isChannelNode(node.value),
            }"
            :aria-level="nodeLevel(node.value)"
            :aria-expanded="isUnitNode(node.value) ? isExpanded(node.value) : undefined"
            :aria-selected="isNodeSelected(node.value)"
            :aria-disabled="isNodeDisabled(node.value)"
            role="treeitem"
            :tabindex="isNodeActive(node.value) ? 0 : -1"
            @keydown="onNodeKeydown($event, node.value)"
            @click="onNodeClick(node.value)"
          >
            <span class="channel-tree-picker__indent" :style="{ width: `${(nodeLevel(node.value) - 1) * 12}px` }"></span>
            <span
              v-if="isUnitNode(node.value)"
              class="channel-tree-picker__toggle"
              :class="{ 'is-expanded': isExpanded(node.value) }"
              aria-hidden="true"
            >
              ▶
            </span>
            <span v-else class="channel-tree-picker__dot" aria-hidden="true">•</span>
            <span class="channel-tree-picker__label">{{ nodeLabel(node.value) }}</span>
            <span
              v-if="isNodeSelected(node.value) && isChannelNode(node.value)"
              class="channel-tree-picker__selected-mark"
              aria-hidden="true"
            >
              ✓
            </span>
            <span
              v-if="isUnitNode(node.value)"
              class="channel-tree-picker__status"
              :class="unitStatusClass(node.value)"
              :title="unitStatusTitle(node.value)"
              aria-hidden="true"
            ></span>
          </button>
        </div>
      </div>
    </Teleport>
  </div>
</template>

<script setup lang="ts">
import { computed, nextTick, ref, watch, type ComponentPublicInstance } from "vue"
import { useFloatingPopover, usePopoverController } from "@affino/popover-vue"
import { useTreeviewController, type TreeviewNode } from "@affino/treeview-vue"

import { useChannelStore } from "@/stores/channelStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"
import type { Channel, ChannelType } from "@/types/channel"
import { APP_OVERLAY_HOST_SELECTOR } from "@/utils/overlayHost"

type NodeValue = string

const props = withDefaults(defineProps<{
  modelValue: number | null
  channelType: ChannelType
  name?: string
  excludeIds?: number[]
  disabled?: boolean
  placeholder?: string
}>(), {
  name: undefined,
  excludeIds: () => [],
  disabled: false,
  placeholder: "— select channel —",
})

const emit = defineEmits<{
  (e: "update:modelValue", value: number | null): void
}>()

const channelStore = useChannelStore()
const deviceStore = useDeviceStore()
const workspaceStore = useWorkspaceStore()

const popover = usePopoverController({
  closeOnInteractOutside: true,
})

const floating = useFloatingPopover(popover, {
  strategy: "fixed",
  placement: "bottom",
  align: "start",
  gutter: 6,
  viewportPadding: 8,
  teleportTo: APP_OVERLAY_HOST_SELECTOR,
  zIndex: 1300,
})

const triggerProps = computed(() => popover.getTriggerProps({ type: "button", role: "dialog" }))
const contentProps = computed(() => popover.getContentProps({ role: "dialog", tabIndex: -1 }))
const popoverContentStyle = computed(() => floating.contentStyle.value)
const popoverTeleportTarget = computed(() => floating.teleportTarget.value)
const catalogLoading = ref(false)

const channelsById = computed(() => {
  const map = new Map<number, Channel>()
  channelStore.channels.forEach((channel) => map.set(channel.id, channel))
  return map
})

const deviceById = computed(() => {
  const map = new Map<number, { unitId: string; status: string }>()
  deviceStore.devices.forEach((device) => {
    map.set(device.id, { unitId: device.unit_id, status: device.status })
  })
  return map
})

const filteredChannels = computed(() => {
  const excluded = new Set(props.excludeIds ?? [])
  const rows = channelStore.channels
    .filter((channel) => channel.type === props.channelType && !excluded.has(channel.id))
    .map((channel) => {
      const device = deviceById.value.get(channel.device_id)
      const unitId = device?.unitId ?? channelStore.resolveUnitId(channel.device_id)
      const online = device?.status === "online" ? 0 : 1
      return {
        id: channel.id,
        channel,
        unitId,
        online,
        status: device?.status ?? "offline",
      }
    })
    .sort((a, b) => {
      if (a.online !== b.online) return a.online - b.online
      const unitCompare = a.unitId.localeCompare(b.unitId)
      if (unitCompare !== 0) return unitCompare
      return a.channel.index - b.channel.index
    })

  return rows
})

const unitStatusById = computed(() => {
  const map = new Map<string, string>()
  filteredChannels.value.forEach((entry) => {
    if (!map.has(entry.unitId)) {
      map.set(entry.unitId, entry.status)
    }
  })
  return map
})

const selectedChannel = computed(() => {
  if (!Number.isFinite(props.modelValue as number)) return null
  return channelsById.value.get(Number(props.modelValue)) ?? null
})

const valueLabel = computed(() => {
  if (!selectedChannel.value) return props.placeholder
  const unitId = channelStore.resolveUnitId(selectedChannel.value.device_id)
  return `${unitId}/ch${selectedChannel.value.index + 1}`
})

const valueClass = computed(() => {
  if (!selectedChannel.value) return "is-empty"
  const status = deviceById.value.get(selectedChannel.value.device_id)?.status ?? "offline"
  return status === "online" ? "is-online" : "is-offline"
})

const tree = useTreeviewController<NodeValue>({
  nodes: [],
  loop: true,
})

const treeNodes = computed<TreeviewNode<NodeValue>[]>(() => {
  const nodes: TreeviewNode<NodeValue>[] = []
  filteredChannels.value.forEach((entry) => {
    const unitValue = toUnitNodeValue(entry.unitId)
    if (!nodes.some((node) => node.value === unitValue)) {
      nodes.push({ value: unitValue, parent: null })
    }
    nodes.push({
      value: toChannelNodeValue(entry.id),
      parent: unitValue,
    })
  })
  return nodes
})

const parentByValue = computed(() => {
  const map = new Map<NodeValue, NodeValue | null>()
  treeNodes.value.forEach((node) => map.set(node.value, node.parent))
  return map
})

const childrenByParent = computed(() => {
  const map = new Map<NodeValue | null, NodeValue[]>()
  treeNodes.value.forEach((node) => {
    const siblings = map.get(node.parent) ?? []
    siblings.push(node.value)
    map.set(node.parent, siblings)
  })
  return map
})

const nodeMeta = computed(() => {
  const map = new Map<NodeValue, { label: string; disabled: boolean }>()
  const grouped = new Map<string, number>()
  filteredChannels.value.forEach((entry) => {
    grouped.set(entry.unitId, (grouped.get(entry.unitId) ?? 0) + 1)
    map.set(toChannelNodeValue(entry.id), {
      label: channelSuffixLabel(channelStore.resolveChannelFullLabel(entry.channel)),
      disabled: false,
    })
  })
  grouped.forEach((count, unitId) => {
    map.set(toUnitNodeValue(unitId), { label: `${unitId} (${count})`, disabled: false })
  })
  return map
})

const expandedSet = computed(() => new Set(tree.state.value.expanded))

const visibleNodes = computed(() => (
  treeNodes.value.filter((node) => isNodeVisible(node.value))
))

const itemElements = new Map<NodeValue, HTMLButtonElement>()

watch(
  treeNodes,
  (nodes) => {
    tree.registerNodes(nodes)
  },
  { immediate: true },
)

watch(
  () => popover.state.value.open,
  async (open) => {
    if (!open) return
    await ensureCatalogLoadedForPicker()
    // Default state: all units collapsed.
    filteredChannels.value.forEach((entry) => {
      tree.collapse(toUnitNodeValue(entry.unitId))
    })
    const hasSelectedNode = syncTreeSelectionFromModelValue()

    await nextTick()
    if (hasSelectedNode) {
      focusNodeElement(tree.state.value.active ?? selectedChannelNodeValueFromModel() ?? "")
    } else {
      const first = visibleNodes.value[0]
      if (first) {
        tree.focus(first.value)
        focusNodeElement(first.value)
      } else {
        focusTreeRoot()
      }
    }

    requestAnimationFrame(() => {
      if (hasSelectedNode) {
        focusNodeElement(tree.state.value.active ?? selectedChannelNodeValueFromModel() ?? "")
      } else {
        const firstNode = visibleNodes.value[0]
        if (firstNode) {
          focusNodeElement(firstNode.value)
        } else {
          focusTreeRoot()
        }
      }
    })
    await floating.updatePosition()
  },
)

watch(
  () => props.modelValue,
  () => {
    if (!popover.state.value.open) return
    syncTreeSelectionFromModelValue()
  },
)

watch(
  () => tree.state.value.active,
  async (active) => {
    if (!active) return
    await nextTick()
    const element = itemElements.get(active)
    if (!element || element === document.activeElement) return
    element.focus({ preventScroll: true })
  },
)

function bindItemElement(value: NodeValue) {
  return (element: Element | ComponentPublicInstance | null) => {
    const resolved = element instanceof Element
      ? element
      : (element?.$el instanceof Element ? element.$el : null)
    if (resolved instanceof HTMLButtonElement) {
      itemElements.set(value, resolved)
      return
    }
    itemElements.delete(value)
  }
}

function focusNodeElement(value: NodeValue) {
  const element = itemElements.get(value)
  if (!element) return
  element.focus({ preventScroll: true })
}

function focusTreeRoot() {
  const root = floating.contentRef.value
  if (!root) return
  root.focus({ preventScroll: true })
}

function toUnitNodeValue(unitId: string): NodeValue {
  return `unit:${unitId}`
}

function toChannelNodeValue(channelId: number): NodeValue {
  return `channel:${channelId}`
}

function isUnitNode(value: NodeValue): boolean {
  return value.startsWith("unit:")
}

function parseUnitId(value: NodeValue): string | null {
  if (!isUnitNode(value)) return null
  const unitId = value.slice("unit:".length).trim()
  return unitId.length > 0 ? unitId : null
}

function isChannelNode(value: NodeValue): boolean {
  return value.startsWith("channel:")
}

function parseChannelId(value: NodeValue): number | null {
  if (!isChannelNode(value)) return null
  const parsed = Number(value.slice("channel:".length))
  return Number.isFinite(parsed) ? parsed : null
}

function channelSuffixLabel(label: string): string {
  const slashIndex = label.indexOf("/")
  if (slashIndex === -1 || slashIndex + 1 >= label.length) return label
  return label.slice(slashIndex + 1)
}

function isNodeVisible(value: NodeValue): boolean {
  let cursor = parentByValue.value.get(value) ?? null
  while (cursor) {
    if (!expandedSet.value.has(cursor)) return false
    cursor = parentByValue.value.get(cursor) ?? null
  }
  return true
}

function nodeLevel(value: NodeValue): number {
  let level = 1
  let cursor = parentByValue.value.get(value) ?? null
  const visited = new Set<NodeValue>()
  while (cursor && !visited.has(cursor)) {
    visited.add(cursor)
    level += 1
    cursor = parentByValue.value.get(cursor) ?? null
  }
  return level
}

function selectedChannelNodeValueFromModel(): NodeValue | null {
  if (!Number.isFinite(props.modelValue as number)) return null
  const channelId = Number(props.modelValue)
  const existsInTree = filteredChannels.value.some(entry => entry.id === channelId)
  if (!existsInTree) return null
  return toChannelNodeValue(channelId)
}

function syncTreeSelectionFromModelValue(): boolean {
  tree.clearSelection()
  const selectedNodeValue = selectedChannelNodeValueFromModel()
  if (!selectedNodeValue) {
    return false
  }
  const parentNode = parentByValue.value.get(selectedNodeValue)
  if (parentNode) {
    tree.expand(parentNode)
  }
  tree.select(selectedNodeValue)
  tree.focus(selectedNodeValue)
  return true
}

function isExpanded(value: NodeValue): boolean {
  return tree.isExpanded(value)
}

function isNodeSelected(value: NodeValue): boolean {
  return tree.isSelected(value)
}

function isNodeActive(value: NodeValue): boolean {
  return tree.isActive(value)
}

function isNodeDisabled(_value: NodeValue): boolean {
  return false
}

function nodeLabel(value: NodeValue): string {
  return nodeMeta.value.get(value)?.label ?? value
}

function unitStatus(value: NodeValue): string {
  const unitId = parseUnitId(value)
  if (!unitId) return "offline"
  const status = String(unitStatusById.value.get(unitId) ?? "").trim().toLowerCase()
  return status === "online" ? "online" : "offline"
}

function unitStatusClass(value: NodeValue): string {
  return unitStatus(value) === "online" ? "is-online" : "is-offline"
}

function unitStatusTitle(value: NodeValue): string {
  return unitStatus(value) === "online" ? "Online" : "Offline"
}

function selectChannel(channelId: number | null) {
  emit("update:modelValue", channelId)
  popover.close("programmatic")
}

function onNodeClick(value: NodeValue) {
  tree.focus(value)
  if (isUnitNode(value)) {
    tree.toggle(value)
    return
  }
  const channelId = parseChannelId(value)
  if (channelId === null) return
  tree.select(value)
  selectChannel(channelId)
}

function onNodeKeydown(event: KeyboardEvent, value: NodeValue) {
  switch (event.key) {
    case "ArrowDown":
      event.preventDefault()
      event.stopPropagation()
      tree.focusNext()
      return
    case "ArrowUp":
      event.preventDefault()
      event.stopPropagation()
      tree.focusPrevious()
      return
    case "Home":
      event.preventDefault()
      event.stopPropagation()
      tree.focusFirst()
      return
    case "End":
      event.preventDefault()
      event.stopPropagation()
      tree.focusLast()
      return
    case "ArrowRight":
      if (isUnitNode(value)) {
        event.preventDefault()
        event.stopPropagation()
        if (!tree.isExpanded(value)) {
          tree.expand(value)
          return
        }
        const firstChild = childrenByParent.value.get(value)?.[0]
        if (firstChild) {
          tree.focus(firstChild)
        }
      }
      return
    case "ArrowLeft":
      if (isUnitNode(value) && tree.isExpanded(value)) {
        event.preventDefault()
        event.stopPropagation()
        tree.collapse(value)
        return
      }
      if (isChannelNode(value)) {
        const parent = parentByValue.value.get(value)
        if (parent) {
          event.preventDefault()
          event.stopPropagation()
          tree.focus(parent)
        }
      }
      return
    case "Enter":
    case " ":
      event.preventDefault()
      event.stopPropagation()
      onNodeClick(value)
      return
    default:
      return
  }
}

function onTreeRootKeydown(event: KeyboardEvent) {
  const active = tree.state.value.active
  switch (event.key) {
    case "ArrowDown":
      event.preventDefault()
      event.stopPropagation()
      if (active) {
        tree.focusNext()
      } else {
        tree.focusFirst()
      }
      return
    case "ArrowUp":
      event.preventDefault()
      event.stopPropagation()
      if (active) {
        tree.focusPrevious()
      } else {
        tree.focusLast()
      }
      return
    case "Home":
      event.preventDefault()
      event.stopPropagation()
      tree.focusFirst()
      return
    case "End":
      event.preventDefault()
      event.stopPropagation()
      tree.focusLast()
      return
    case "ArrowRight":
      if (!active) return
      event.preventDefault()
      event.stopPropagation()
      if (isUnitNode(active)) {
        if (!tree.isExpanded(active)) {
          tree.expand(active)
          return
        }
        const firstChild = childrenByParent.value.get(active)?.[0]
        if (firstChild) {
          tree.focus(firstChild)
        }
      }
      return
    case "ArrowLeft":
      if (!active) return
      event.preventDefault()
      event.stopPropagation()
      if (isUnitNode(active) && tree.isExpanded(active)) {
        tree.collapse(active)
        return
      }
      if (isChannelNode(active)) {
        const parent = parentByValue.value.get(active)
        if (parent) {
          tree.focus(parent)
        }
      }
      return
    case "Enter":
    case " ":
      if (!active) return
      event.preventDefault()
      event.stopPropagation()
      onNodeClick(active)
      return
    default:
      return
  }
}

function handleClear() {
  selectChannel(null)
}

async function ensureCatalogLoadedForPicker() {
  if (catalogLoading.value) {
    return
  }
  catalogLoading.value = true
  try {
    const requiredType = String(props.channelType ?? "").trim().toLowerCase() || "any"
    const workspaceId = workspaceStore.activeWorkspaceId ?? "none"

    await runStoreBootstrap(
      ["channel-tree-picker", workspaceId, requiredType],
      [
        () => deviceStore.ensureLoaded(),
        () => {
          const candidates = deviceStore.devices
            .filter((device) => requiredType === "any" || String(device.device_type ?? "").trim().toLowerCase() === requiredType)
            .map(device => device.id)

          if (candidates.length > 0) {
            return Promise.allSettled(
              candidates.map(deviceId => channelStore.ensureDeviceChannelsLoaded(deviceId)),
            )
          }

          if (channelStore.channels.length === 0 && deviceStore.devices.length > 0) {
            return Promise.allSettled(
              deviceStore.devices.map(device => channelStore.ensureDeviceChannelsLoaded(device.id)),
            )
          }
        },
      ],
      { mode: "settled" },
    )
  } finally {
    catalogLoading.value = false
  }
}
</script>

<style scoped>
.channel-tree-picker {
  width: 100%;
}

.channel-tree-picker__trigger {
  display: inline-flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
  min-height: 32px;
  border: 1px solid rgb(212 212 212);
  border-radius: 8px;
  background: rgb(255 255 255);
  padding: 5px 10px;
  font-size: 12px;
  text-align: left;
  color: rgb(23 23 23);
}

.dark .channel-tree-picker__trigger {
  border-color: rgb(82 82 91);
  background: rgb(24 24 27);
  color: rgb(245 245 245);
}

.channel-tree-picker__trigger:focus,
.channel-tree-picker__trigger:focus-visible,
.channel-tree-picker__clear:focus,
.channel-tree-picker__clear:focus-visible,
.channel-tree-picker__node:focus,
.channel-tree-picker__node:focus-visible {
  outline: none;
  box-shadow: none;
}

.channel-tree-picker__trigger:disabled {
  opacity: 0.55;
  cursor: default;
}

.channel-tree-picker__value {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  padding-right: 8px;
}

.channel-tree-picker__value.is-empty {
  color: rgb(115 115 115);
}

.channel-tree-picker__value.is-online {
  color: rgb(23 23 23);
  font-weight: 600;
}

.channel-tree-picker__value.is-offline {
  color: rgb(115 115 115);
  font-weight: 600;
}

.dark .channel-tree-picker__value.is-empty {
  color: rgb(161 161 170);
}

.dark .channel-tree-picker__value.is-online {
  color: rgb(245 245 245);
}

.dark .channel-tree-picker__value.is-offline {
  color: rgb(161 161 170);
}

.channel-tree-picker__chevron {
  flex: 0 0 auto;
  color: rgb(115 115 115);
  font-size: 10px;
}

.channel-tree-picker__popover {
  width: min(360px, calc(100vw - 24px));
  max-height: min(460px, calc(100vh - 32px));
  overflow: auto;
  border: 1px solid rgb(229 229 229);
  border-radius: 12px;
  background: rgb(255 255 255);
  box-shadow: 0 16px 28px rgb(15 23 42 / 16%);
  padding: 8px;
}

.dark .channel-tree-picker__popover {
  border-color: rgb(64 64 64);
  background: rgb(24 24 27);
}

.channel-tree-picker__header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  padding: 4px 6px 8px;
}

.channel-tree-picker__title {
  font-size: 11px;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: rgb(115 115 115);
}

.channel-tree-picker__clear {
  border: 1px solid rgb(212 212 212);
  border-radius: 8px;
  background: rgb(250 250 250);
  padding: 2px 8px;
  font-size: 11px;
  font-weight: 600;
  color: rgb(63 63 70);
}

.channel-tree-picker__clear:hover {
  background: rgb(245 245 245);
}

.dark .channel-tree-picker__clear {
  border-color: rgb(82 82 91);
  background: rgb(39 39 42);
  color: rgb(228 228 231);
}

.channel-tree-picker__tree {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.channel-tree-picker__node {
  display: inline-flex;
  align-items: center;
  width: 100%;
  height: 28px;
  border: 0;
  border-radius: 8px;
  background: transparent;
  text-align: left;
  padding: 0 6px;
  color: rgb(23 23 23);
  transition: background-color 120ms ease, box-shadow 120ms ease, color 120ms ease;
}

.channel-tree-picker__node:hover {
  background: rgb(245 245 245);
}

.channel-tree-picker__node.is-active {
  background: rgb(241 245 249);
  box-shadow: inset 0 0 0 1px rgb(186 230 253);
}

.channel-tree-picker__node.is-selected {
  background: rgb(238 242 255);
  box-shadow: inset 0 0 0 1px rgb(165 180 252);
}

.channel-tree-picker__node.is-selected.is-active {
  background: rgb(224 231 255);
  box-shadow: inset 0 0 0 1px rgb(129 140 248);
}

.channel-tree-picker__node.is-selected .channel-tree-picker__label {
  font-weight: 700;
}

.dark .channel-tree-picker__node {
  color: rgb(245 245 245);
}

.dark .channel-tree-picker__node:hover {
  background: rgb(39 39 42);
}

.dark .channel-tree-picker__node.is-active {
  background: rgb(31 41 55);
  box-shadow: inset 0 0 0 1px rgb(59 130 246 / 60%);
}

.dark .channel-tree-picker__node.is-selected {
  background: rgb(37 44 65);
  box-shadow: inset 0 0 0 1px rgb(129 140 248 / 65%);
}

.dark .channel-tree-picker__node.is-selected.is-active {
  background: rgb(41 50 79);
  box-shadow: inset 0 0 0 1px rgb(129 140 248 / 70%);
}

.channel-tree-picker__selected-mark {
  margin-left: 8px;
  margin-right: 2px;
  font-size: 11px;
  font-weight: 700;
  color: rgb(79 70 229);
}

.dark .channel-tree-picker__selected-mark {
  color: rgb(165 180 252);
}

.channel-tree-picker__indent {
  flex: 0 0 auto;
}

.channel-tree-picker__toggle,
.channel-tree-picker__dot {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 14px;
  min-width: 14px;
  margin-right: 6px;
  font-size: 10px;
  color: rgb(115 115 115);
}

.channel-tree-picker__toggle {
  transition: transform 120ms ease;
}

.channel-tree-picker__toggle.is-expanded {
  transform: rotate(90deg);
}

.channel-tree-picker__label {
  display: inline-block;
  flex: 1 1 auto;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-size: 12px;
}

.channel-tree-picker__status {
  width: 7px;
  height: 7px;
  min-width: 7px;
  border-radius: 999px;
  margin-left: 8px;
  margin-right: 2px;
}

.channel-tree-picker__status.is-online {
  background: rgb(34 197 94);
}

.channel-tree-picker__status.is-offline {
  background: rgb(113 113 122);
}

.dark .channel-tree-picker__status.is-online {
  background: rgb(74 222 128);
}

.dark .channel-tree-picker__status.is-offline {
  background: rgb(82 82 91);
}

.channel-tree-picker__empty {
  padding: 12px 8px;
  text-align: center;
  font-size: 12px;
  color: rgb(115 115 115);
}
</style>
