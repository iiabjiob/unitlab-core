<template>
  <div class="flex min-w-0 items-center justify-between gap-2">
    <span class="truncate text-xs" :class="valueClass" :title="valueLabel">
      {{ valueLabel }}
    </span>

    <button
      :ref="floating.triggerRef"
      type="button"
      class="allocation-picker__trigger shrink-0 rounded border border-neutral-300 bg-white px-2 py-1 text-[11px] font-semibold text-neutral-700 hover:bg-neutral-100 disabled:cursor-default disabled:opacity-50 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200 dark:hover:bg-neutral-800"
      :disabled="loading"
      v-bind="triggerProps"
      @click.stop
    >
      {{ row.channel_id ? "Change" : "Bind" }}
    </button>
  </div>

  <Teleport v-if="popover.state.value.open && popoverTeleportTarget" :to="popoverTeleportTarget">
    <div
      :ref="floating.contentRef"
      class="allocation-picker"
      :style="popoverContentStyle"
      v-bind="contentProps"
      @click.stop
      @keydown="onTreeRootKeydown"
    >
      <div class="allocation-picker__header">
        <span class="allocation-picker__title">Allocate channel</span>
        <button
          v-if="row.channel_id"
          type="button"
          class="allocation-picker__unassign"
          :disabled="loading"
          @click="handleUnassign"
        >
          Unassign
        </button>
      </div>

      <div v-if="visibleNodes.length === 0" class="allocation-picker__empty">
        No compatible channels
      </div>

      <div
        v-else
        class="allocation-picker__tree"
        role="tree"
        aria-label="Channel allocation tree"
      >
        <button
          v-for="node in visibleNodes"
          :key="node.value"
          :ref="bindItemElement(node.value)"
          type="button"
          class="allocation-picker__node"
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
          <span class="allocation-picker__indent" :style="{ width: `${(nodeLevel(node.value) - 1) * 12}px` }"></span>
          <span
            v-if="isUnitNode(node.value)"
            class="allocation-picker__toggle"
            :class="{ 'is-expanded': isExpanded(node.value) }"
            aria-hidden="true"
          >
            ▶
          </span>
          <span v-else class="allocation-picker__dot" aria-hidden="true">•</span>
          <span class="allocation-picker__label">{{ nodeLabel(node.value) }}</span>
          <span
            v-if="isNodeSelected(node.value) && isChannelNode(node.value)"
            class="allocation-picker__selected-mark"
            aria-hidden="true"
          >
            ✓
          </span>
          <span
            v-if="isUnitNode(node.value)"
            class="allocation-picker__unit-status"
            :class="unitStatusClass(node.value)"
            :title="unitStatusTitle(node.value)"
            aria-hidden="true"
          ></span>
        </button>
      </div>
    </div>
  </Teleport>
</template>

<script setup lang="ts">
import { computed, nextTick, ref, watch, type ComponentPublicInstance } from "vue"
import { useFloatingPopover, usePopoverController } from "@affino/popover-vue"
import { useTreeviewController, type TreeviewNode } from "@affino/treeview-vue"

import type { SignalAllocationRow } from "@/types/signal"

type ChannelOption = { id: number; label: string; disabled: boolean }
type ChannelOptionGroup = { unitId: string; options: ChannelOption[] }
type NodeValue = string

const props = defineProps<{
  row: SignalAllocationRow
  loading: boolean
  valueLabel: string
  valueClass: string
  channelGroups?: ChannelOptionGroup[]
  channelGroupsResolver?: () => ChannelOptionGroup[]
  unitStatusById: Record<string, string>
}>()

const emit = defineEmits<{
  (e: "allocate", channelId: number | null): void
}>()

const popover = usePopoverController({
  closeOnInteractOutside: true,
})

const floating = useFloatingPopover(popover, {
  strategy: "fixed",
  placement: "bottom",
  align: "end",
  gutter: 6,
  viewportPadding: 8,
  zIndex: 1300,
})

const triggerProps = computed(() => popover.getTriggerProps({ type: "button", role: "dialog" }))
const contentProps = computed(() => popover.getContentProps({ role: "dialog", tabIndex: -1 }))
const popoverContentStyle = computed(() => floating.contentStyle.value)
const popoverTeleportTarget = computed(() => floating.teleportTarget.value)

const tree = useTreeviewController<NodeValue>({
  nodes: [],
  loop: true,
})

const activeChannelGroups = ref<ChannelOptionGroup[]>([])

function resolveChannelGroups(): ChannelOptionGroup[] {
  if (typeof props.channelGroupsResolver === "function") {
    const resolved = props.channelGroupsResolver()
    return Array.isArray(resolved) ? resolved : []
  }
  return Array.isArray(props.channelGroups) ? props.channelGroups : []
}

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
  activeChannelGroups.value.forEach((group) => {
    const unitValue = toUnitNodeValue(group.unitId)
    map.set(unitValue, { label: `${group.unitId} (${group.options.length})`, disabled: false })
    group.options.forEach((option) => {
      const channelValue = toChannelNodeValue(option.id)
      map.set(channelValue, {
        label: option.label,
        disabled: option.disabled,
      })
    })
  })
  return map
})

const expandedSet = computed(() => new Set(tree.state.value.expanded))

const treeNodes = computed<TreeviewNode<NodeValue>[]>(() => {
  const nodes: TreeviewNode<NodeValue>[] = []
  activeChannelGroups.value.forEach((group) => {
    const unitValue = toUnitNodeValue(group.unitId)
    nodes.push({ value: unitValue, parent: null })
    group.options.forEach((option) => {
      nodes.push({
        value: toChannelNodeValue(option.id),
        parent: unitValue,
        disabled: option.disabled,
      })
    })
  })
  return nodes
})

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
    if (!open) {
      activeChannelGroups.value = []
      return
    }
    activeChannelGroups.value = resolveChannelGroups()
    await nextTick()
    tree.registerNodes(treeNodes.value)
    // Default state: all unit nodes collapsed.
    activeChannelGroups.value.forEach((group) => {
      tree.collapse(toUnitNodeValue(group.unitId))
    })
    tree.clearSelection()

    const selectedNode = selectedChannelNodeValueFromRow()
    if (selectedNode) {
      const parent = parentByValue.value.get(selectedNode)
      if (parent) {
        tree.expand(parent)
      }
      tree.select(selectedNode)
      tree.focus(selectedNode)
    }

    await nextTick()
    if (selectedNode) {
      focusNodeElement(selectedNode)
    } else {
      const first = visibleNodes.value[0]
      if (first) {
        tree.focus(first.value)
        focusNodeElement(first.value)
      } else {
        focusTreeRoot()
      }
    }
    // Teleported content can mount one frame later in some cases.
    requestAnimationFrame(() => {
      if (selectedNode) {
        focusNodeElement(selectedNode)
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
  () => tree.state.value.active,
  async (active) => {
    if (!active) return
    await nextTick()
    const element = itemElements.get(active)
    if (!element || element === document.activeElement) return
    element.focus({ preventScroll: true })
    element.scrollIntoView({ block: "nearest" })
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
  element.scrollIntoView({ block: "nearest" })
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

function selectedChannelNodeValueFromRow(): NodeValue | null {
  const channelId = Number(props.row.channel_id)
  if (!Number.isFinite(channelId) || channelId <= 0) return null
  const value = toChannelNodeValue(channelId)
  return treeNodes.value.some(node => node.value === value) ? value : null
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

function isExpanded(value: NodeValue): boolean {
  return tree.isExpanded(value)
}

function isNodeSelected(value: NodeValue): boolean {
  return tree.isSelected(value)
}

function isNodeActive(value: NodeValue): boolean {
  return tree.isActive(value)
}

function isNodeDisabled(value: NodeValue): boolean {
  return Boolean(nodeMeta.value.get(value)?.disabled)
}

function nodeLabel(value: NodeValue): string {
  return nodeMeta.value.get(value)?.label ?? value
}

function unitStatus(value: NodeValue): string {
  const unitId = parseUnitId(value)
  if (!unitId) return "offline"
  const status = String(props.unitStatusById[unitId] ?? "").trim().toLowerCase()
  return status === "online" ? "online" : "offline"
}

function unitStatusClass(value: NodeValue): string {
  return unitStatus(value) === "online" ? "is-online" : "is-offline"
}

function unitStatusTitle(value: NodeValue): string {
  return unitStatus(value) === "online" ? "Online" : "Offline"
}

function onNodeClick(value: NodeValue) {
  tree.focus(value)
  if (isUnitNode(value)) {
    tree.toggle(value)
    return
  }
  if (isNodeDisabled(value)) return
  const channelId = parseChannelId(value)
  if (channelId === null) return
  tree.select(value)
  emit("allocate", channelId)
  popover.close("programmatic")
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

function handleUnassign() {
  emit("allocate", null)
  popover.close("programmatic")
}
</script>

<style scoped>
.allocation-picker {
  width: min(360px, calc(100vw - 24px));
  max-height: min(460px, calc(100vh - 32px));
  overflow: auto;
  border: 1px solid rgb(229 229 229);
  border-radius: 12px;
  background: rgb(255 255 255);
  box-shadow: 0 16px 28px rgb(15 23 42 / 16%);
  padding: 8px;
}

.allocation-picker:focus,
.allocation-picker:focus-visible {
  outline: none;
}

.allocation-picker__trigger:focus,
.allocation-picker__trigger:focus-visible,
.allocation-picker__unassign:focus,
.allocation-picker__unassign:focus-visible,
.allocation-picker__node:focus,
.allocation-picker__node:focus-visible {
  outline: none;
  box-shadow: none;
}

.dark .allocation-picker {
  border-color: rgb(64 64 64);
  background: rgb(24 24 27);
}

.allocation-picker__header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  padding: 4px 6px 8px;
}

.allocation-picker__title {
  font-size: 11px;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: rgb(115 115 115);
}

.allocation-picker__unassign {
  border: 1px solid rgb(212 212 212);
  border-radius: 8px;
  background: rgb(250 250 250);
  padding: 2px 8px;
  font-size: 11px;
  font-weight: 600;
  color: rgb(63 63 70);
}

.allocation-picker__unassign:hover {
  background: rgb(245 245 245);
}

.dark .allocation-picker__unassign {
  border-color: rgb(82 82 91);
  background: rgb(39 39 42);
  color: rgb(228 228 231);
}

.allocation-picker__tree {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.allocation-picker__node {
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

.allocation-picker__node:hover {
  background: rgb(245 245 245);
}

.allocation-picker__node.is-active {
  background: rgb(241 245 249);
  box-shadow: inset 0 0 0 1px rgb(186 230 253);
}

.allocation-picker__node.is-selected {
  background: rgb(224 231 255);
  box-shadow: inset 0 0 0 1px rgb(129 140 248 / 60%);
}

.allocation-picker__node.is-selected.is-active {
  background: rgb(199 210 254);
  box-shadow: inset 0 0 0 1px rgb(129 140 248);
}

.allocation-picker__node.is-selected .allocation-picker__label {
  font-weight: 700;
}

.allocation-picker__node.is-disabled {
  color: rgb(161 161 170);
}

.dark .allocation-picker__node {
  color: rgb(245 245 245);
}

.dark .allocation-picker__node:hover {
  background: rgb(39 39 42);
}

.dark .allocation-picker__node.is-active {
  background: rgb(38 38 38);
  box-shadow: inset 0 0 0 1px rgb(115 115 115 / 55%);
}

.dark .allocation-picker__node.is-selected {
  background: rgb(50 50 50);
  box-shadow: inset 0 0 0 1px rgb(148 148 148 / 60%);
}

.dark .allocation-picker__node.is-selected.is-active {
  background: rgb(64 64 64);
  box-shadow: inset 0 0 0 1px rgb(163 163 163 / 70%);
}

.dark .allocation-picker__node.is-disabled {
  color: rgb(113 113 122);
}

.allocation-picker__indent {
  flex: 0 0 auto;
}

.allocation-picker__toggle,
.allocation-picker__dot {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 14px;
  min-width: 14px;
  margin-right: 6px;
  font-size: 10px;
  color: rgb(115 115 115);
}

.allocation-picker__toggle {
  transition: transform 120ms ease;
}

.allocation-picker__toggle.is-expanded {
  transform: rotate(90deg);
}

.allocation-picker__label {
  display: inline-block;
  flex: 1 1 auto;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-size: 12px;
}

.allocation-picker__selected-mark {
  margin-right: 6px;
  font-size: 11px;
  font-weight: 700;
  color: rgb(79 70 229);
}

.dark .allocation-picker__selected-mark {
  color: rgb(212 212 212);
}

.allocation-picker__unit-status {
  width: 7px;
  height: 7px;
  min-width: 7px;
  border-radius: 999px;
  margin-left: 8px;
  margin-right: 2px;
}

.allocation-picker__unit-status.is-online {
  background: rgb(34 197 94);
}

.allocation-picker__unit-status.is-offline {
  background: rgb(113 113 122);
}

.dark .allocation-picker__unit-status.is-online {
  background: rgb(74 222 128);
}

.dark .allocation-picker__unit-status.is-offline {
  background: rgb(82 82 91);
}

.allocation-picker__empty {
  padding: 12px 8px;
  text-align: center;
  font-size: 12px;
  color: rgb(115 115 115);
}
</style>
