<template>
  <SlideOver
    :open="open"
    placement="right"
    title="Assign unit/channel"
    :width-px="560"
    :close-on-backdrop="!saving"
    @close="emit('close')"
  >
    <div class="allocation-channel-picker" @keydown.capture="onPanelKeydownCapture">
      <div class="allocation-channel-picker__header">
        <div class="allocation-channel-picker__eyebrow">
          {{ signalDirection }} signal
        </div>

        <div class="allocation-channel-picker__current">
          <div class="allocation-channel-picker__eyebrow">
            Current allocation
          </div>
          <div class="allocation-channel-picker__current-label">
            {{ currentLabel }}
          </div>
        </div>

        <label class="allocation-channel-picker__search">
          <span class="allocation-channel-picker__search-label">Search channels</span>
          <input
            ref="searchInputRef"
            v-model="query"
            data-dialog-initial
            type="text"
            autocomplete="off"
            spellcheck="false"
            class="allocation-channel-picker__search-input"
            placeholder="Filter by unit, channel, or name"
            :disabled="loading || saving"
            @focus="treeDomFocusActive = false"
            @keydown.down.prevent="focusNextVisibleNode()"
            @keydown.up.prevent="focusPreviousVisibleNode()"
            @keydown.enter.prevent="selectActiveNode()"
          />
        </label>

        <div class="allocation-channel-picker__summary-row">
          <span>{{ resultsSummary }}</span>
            <button
              v-if="currentChannelId !== null"
              type="button"
              class="btn btn-xs btn-secondary allocation-channel-picker__clear-button"
              :disabled="saving"
            @click="emit('select', null)"
          >
            Clear allocation
          </button>
        </div>
      </div>

      <div class="allocation-channel-picker__body">
        <div v-if="loading" class="allocation-channel-picker__empty">
          Loading channel catalog…
        </div>

        <div v-else-if="error" class="allocation-channel-picker__error">
          {{ error }}
        </div>

        <div v-else-if="visibleNodes.length === 0" class="allocation-channel-picker__empty">
          No compatible channels found.
        </div>

        <div
          v-else
          class="allocation-channel-picker__tree"
          role="tree"
          aria-label="Channel tree"
          @keydown="onTreeRootKeydown"
        >
          <div
            v-for="node in visibleNodes"
            :key="node.value"
            :ref="bindItemElement(node.value)"
            class="allocation-channel-picker__tree-node"
            :class="[nodeClass(node.value), nodeCursorClass(node.value)]"
            :aria-level="nodeLevel(node.value)"
            :aria-expanded="isUnitNode(node.value) ? isExpanded(node.value) : undefined"
            :aria-selected="isChannelNode(node.value) ? isNodeSelected(node.value) : undefined"
            role="treeitem"
            :tabindex="isNodeActive(node.value) ? 0 : -1"
            @focus="handleTreeNodeFocus(node.value)"
            @keydown="onNodeKeydown($event, node.value)"
            @click="onNodeClick(node.value)"
          >
            <span class="allocation-channel-picker__indent" :style="{ width: `${(nodeLevel(node.value) - 1) * 14}px` }"></span>
            <span
              v-if="isUnitNode(node.value)"
              class="allocation-channel-picker__chevron"
              :class="isExpanded(node.value) ? 'allocation-channel-picker__chevron--expanded' : ''"
              aria-hidden="true"
            >
              ▶
            </span>
            <span
              v-else-if="isChannelSaving(node.value)"
              class="allocation-channel-picker__spinner"
              aria-hidden="true"
            ></span>
            <span v-else class="allocation-channel-picker__channel-dot" :class="channelIndicatorClass(node.value)" aria-hidden="true"></span>
            <span
              v-if="isUnitNode(node.value)"
              class="allocation-channel-picker__unit-dot"
              :class="unitIndicatorClass(node.value)"
              aria-hidden="true"
            ></span>
            <span
              class="allocation-channel-picker__node-label"
              :class="[
                isUnitNode(node.value) ? 'allocation-channel-picker__node-label--unit' : 'allocation-channel-picker__node-label--channel',
                channelOwnerRowText(node.value) ? 'allocation-channel-picker__node-label--with-owner' : 'allocation-channel-picker__node-label--grow',
              ]"
            >
              {{ nodeLabel(node.value) }}
            </span>
            <UiHoverTooltip
              v-if="isChannelNode(node.value) && channelOwnerRowText(node.value)"
              :text="channelOwnerRowText(node.value)"
              :open-delay="2000"
              placement="left"
              align="center"
              multiline
              :z-index="1100"
              :open-on-focus="false"
              suppress-open-on-click
              strict-trigger-hover
              v-slot="{ setTriggerRef, getTriggerProps }"
            >
              <span
                :ref="setTriggerRef"
                class="allocation-channel-picker__owner"
                v-bind="getTriggerProps()"
                @click.stop
                @mousedown.stop
              >
                {{ channelOwnerRowText(node.value) }}
              </span>
            </UiHoverTooltip>
            <span
              v-if="isChannelNode(node.value) && isCurrentChannel(node.value)"
              class="allocation-channel-picker__tag allocation-channel-picker__tag--current"
            >
              Current
            </span>
            <button
              v-else-if="isChannelNode(node.value) && isOccupiedChannel(node.value)"
              type="button"
              class="btn btn-xs btn-secondary allocation-channel-picker__swap-button"
              :disabled="saving"
              tabindex="-1"
              @click.stop="onSwapClick(node.value)"
              @keydown.enter.stop.prevent="onSwapClick(node.value)"
              @keydown.space.stop.prevent="onSwapClick(node.value)"
            >
              Swap
            </button>
          </div>
        </div>
      </div>
    </div>
  </SlideOver>
</template>

<script setup lang="ts">
import { computed, nextTick, ref, watch, type ComponentPublicInstance } from "vue"
import { useTreeviewController, type TreeviewNode } from "@affino/treeview-vue"

import SlideOver from "@/components/ui/SlideOver.vue"
import UiHoverTooltip from "@/components/ui/UiHoverTooltip.vue"

type AllocationChannelCandidate = {
  id: number
  unitId: string
  unitLabel: string
  channelIndex: number
  channelLabel: string
  online: boolean
  occupied: boolean
  ownerSignalId: number | null
  ownerRowText: string | null
  searchText: string
}

type NodeValue = string

const props = defineProps<{
  open: boolean
  signalName: string
  signalKey: string
  signalDirection: string
  currentLabel: string
  currentChannelId: number | null
  channels: AllocationChannelCandidate[]
  loading: boolean
  saving: boolean
  error: string | null
}>()

const emit = defineEmits<{
  (event: "close"): void
  (event: "select", channelId: number | null): void
}>()

const query = ref("")
const searchInputRef = ref<HTMLInputElement | null>(null)
const itemElements = new Map<NodeValue, HTMLElement>()
const treeDomFocusActive = ref(false)
const pendingCurrentChannelReveal = ref(false)
const tree = useTreeviewController<NodeValue>({
  nodes: [],
  loop: true,
})

const normalizedQuery = computed(() => query.value.trim().toLowerCase())

const matchingChannels = computed(() => {
  const needle = normalizedQuery.value
  if (!needle) {
    return props.channels
  }
  return props.channels.filter(channel => channel.searchText.includes(needle))
})

const groupedChannels = computed(() => {
  const groups = new Map<string, AllocationChannelCandidate[]>()
  matchingChannels.value.forEach((channel) => {
    const bucket = groups.get(channel.unitId) ?? []
    bucket.push(channel)
    groups.set(channel.unitId, bucket)
  })
  return Array.from(groups.entries())
    .map(([unitId, entries]) => ({
      unitId,
      entries: [...entries].sort((left, right) => left.channelIndex - right.channelIndex),
    }))
    .sort((left, right) => left.unitId.localeCompare(right.unitId, undefined, { numeric: true, sensitivity: "base" }))
})

const treeNodes = computed<TreeviewNode<NodeValue>[]>(() => {
  const nodes: TreeviewNode<NodeValue>[] = []
  groupedChannels.value.forEach((group) => {
    const unitValue = toUnitNodeValue(group.unitId)
    nodes.push({ value: unitValue, parent: null })
    group.entries.forEach((channel) => {
      nodes.push({
        value: toChannelNodeValue(channel.id),
        parent: unitValue,
      })
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
    const bucket = map.get(node.parent) ?? []
    bucket.push(node.value)
    map.set(node.parent, bucket)
  })
  return map
})

const channelById = computed(() => {
  const map = new Map<number, AllocationChannelCandidate>()
  matchingChannels.value.forEach((channel) => {
    map.set(channel.id, channel)
  })
  return map
})

const unitHasFreeChannelById = computed(() => {
  const map = new Map<string, boolean>()
  groupedChannels.value.forEach((group) => {
    map.set(group.unitId, group.entries.some(entry => !entry.occupied))
  })
  return map
})

const nodeMeta = computed(() => {
  const map = new Map<NodeValue, { label: string }>()
  groupedChannels.value.forEach((group) => {
    map.set(toUnitNodeValue(group.unitId), {
      label: group.entries[0]?.unitLabel || group.unitId,
    })
    group.entries.forEach((channel) => {
      map.set(toChannelNodeValue(channel.id), {
        label: channel.channelLabel,
      })
    })
  })
  return map
})

const treeNodeByValue = computed(() => {
  const map = new Map<NodeValue, TreeviewNode<NodeValue>>()
  treeNodes.value.forEach((node) => {
    map.set(node.value, node)
  })
  return map
})

const visibleNodeValues = computed(() => {
  void tree.state.value
  return tree.getVisibleValues()
})

const visibleNodes = computed(() => (
  visibleNodeValues.value
    .map(value => treeNodeByValue.value.get(value))
    .filter((node): node is TreeviewNode<NodeValue> => Boolean(node))
))

const resultsSummary = computed(() => {
  const units = groupedChannels.value.length
  const channels = matchingChannels.value.length
  if (props.loading) return "Loading channels…"
  if (channels === 0) return "No compatible channels"
  return `${units} unit${units === 1 ? "" : "s"} · ${channels} channel${channels === 1 ? "" : "s"}`
})

watch(treeNodes, (nodes) => {
  tree.registerNodes(nodes)
  if (!props.open) {
    return
  }
  syncTreeState()
  if (pendingCurrentChannelReveal.value) {
    void nextTick(() => {
      revealPendingCurrentChannel()
    })
  }
}, { immediate: true })

watch(
  () => props.open,
  (open) => {
    if (!open) {
      query.value = ""
      pendingCurrentChannelReveal.value = false
      return
    }
    pendingCurrentChannelReveal.value = props.currentChannelId !== null
    syncTreeState()
    void nextTick(() => {
      revealPendingCurrentChannel()
      searchInputRef.value?.focus({ preventScroll: true })
    })
  },
)

watch(
  () => [normalizedQuery.value, props.currentChannelId] as const,
  () => {
    if (!props.open) return
    syncTreeState()
    if (pendingCurrentChannelReveal.value) {
      void nextTick(() => {
        revealPendingCurrentChannel()
      })
    }
  },
)

watch(
  () => tree.state.value.active,
  (active) => {
    if (!treeDomFocusActive.value || !active) {
      return
    }
    void nextTick(() => {
      focusNodeElement(active)
    })
  },
)

function toUnitNodeValue(unitId: string): NodeValue {
  return `unit:${unitId}`
}

function toChannelNodeValue(channelId: number): NodeValue {
  return `channel:${channelId}`
}

function isUnitNode(value: NodeValue): boolean {
  return value.startsWith("unit:")
}

function isChannelNode(value: NodeValue): boolean {
  return value.startsWith("channel:")
}

function parseUnitId(value: NodeValue): string | null {
  if (!isUnitNode(value)) return null
  const unitId = value.slice("unit:".length).trim()
  return unitId || null
}

function parseChannelId(value: NodeValue): number | null {
  if (!isChannelNode(value)) return null
  const parsed = Number(value.slice("channel:".length))
  return Number.isFinite(parsed) ? parsed : null
}

function bindItemElement(value: NodeValue) {
  return (element: Element | ComponentPublicInstance | null) => {
    const resolved = element instanceof Element
      ? element
      : (element?.$el instanceof Element ? element.$el : null)
    if (resolved instanceof HTMLElement) {
      itemElements.set(value, resolved)
      return
    }
    itemElements.delete(value)
  }
}

function handleTreeNodeFocus(value: NodeValue) {
  treeDomFocusActive.value = true
  tree.focus(value)
}

function focusNodeElement(value: NodeValue | null) {
  if (!value) return
  const element = itemElements.get(value)
  if (!element) return
  element.focus({ preventScroll: true })
  element.scrollIntoView({ block: "nearest" })
}

function revealNodeElement(value: NodeValue | null) {
  if (!value) return false
  const element = itemElements.get(value)
  if (!element) return false
  element.scrollIntoView({ block: "center", inline: "nearest" })
  return true
}

function revealPendingCurrentChannel() {
  if (!pendingCurrentChannelReveal.value || props.currentChannelId === null) {
    return
  }

  const currentNode = syncCurrentChannelNode()
  if (!currentNode || !visibleNodeValues.value.includes(currentNode)) {
    return
  }

  if (revealNodeElement(currentNode)) {
    pendingCurrentChannelReveal.value = false
  }
}

function currentChannelNodeValue(): NodeValue | null {
  if (props.currentChannelId === null) {
    return null
  }
  return toChannelNodeValue(props.currentChannelId)
}

function hasTreeNode(value: NodeValue | null): value is NodeValue {
  return value !== null && treeNodeByValue.value.has(value)
}

function syncCurrentChannelNode(): NodeValue | null {
  const currentNode = currentChannelNodeValue()
  if (!hasTreeNode(currentNode)) {
    return null
  }

  tree.clearSelection()
  const result = tree.core.requestSelect(currentNode)
  return result.ok ? currentNode : null
}

function syncTreeState() {
  groupedChannels.value.forEach((group) => {
    tree.collapse(toUnitNodeValue(group.unitId))
  })

  if (normalizedQuery.value) {
    groupedChannels.value.forEach((group) => {
      tree.expand(toUnitNodeValue(group.unitId))
    })
  }

  if (syncCurrentChannelNode()) {
    return
  }

  tree.clearSelection()
  const first = visibleNodeValues.value[0] ?? null
  if (first) {
    tree.focus(first)
  }
}

function nodeLevel(value: NodeValue): number {
  let level = 1
  let cursor = parentByValue.value.get(value) ?? null
  while (cursor) {
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

function nodeLabel(value: NodeValue): string {
  return nodeMeta.value.get(value)?.label ?? value
}

function isCurrentChannel(value: NodeValue): boolean {
  const channelId = parseChannelId(value)
  return channelId !== null && channelId === props.currentChannelId
}

function isOccupiedChannel(value: NodeValue): boolean {
  const channelId = parseChannelId(value)
  if (channelId === null) return false
  return Boolean(channelById.value.get(channelId)?.occupied)
}

function requiresSwapAction(value: NodeValue): boolean {
  return isChannelNode(value) && isOccupiedChannel(value) && !isCurrentChannel(value)
}

function isChannelSaving(value: NodeValue): boolean {
  return props.saving && isChannelNode(value) && isNodeSelected(value)
}

function channelOwnerRowText(value: NodeValue): string {
  const channelId = parseChannelId(value)
  if (channelId === null) return ""
  const channel = channelById.value.get(channelId)
  if (!channel?.occupied || !channel.ownerRowText) return ""
  return channel.ownerRowText
}

function channelIndicatorClass(value: NodeValue): string {
  const channelId = parseChannelId(value)
  if (channelId === null) return "allocation-channel-picker__channel-dot--unknown"
  const channel = channelById.value.get(channelId)
  if (!channel) return "allocation-channel-picker__channel-dot--unknown"
  return channel.occupied
    ? "allocation-channel-picker__channel-dot--occupied"
    : "allocation-channel-picker__channel-dot--free"
}

function unitIndicatorClass(value: NodeValue): string {
  const unitId = parseUnitId(value)
  if (!unitId) return "allocation-channel-picker__unit-dot--unknown"
  return unitHasFreeChannelById.value.get(unitId)
    ? "allocation-channel-picker__unit-dot--free"
    : "allocation-channel-picker__unit-dot--occupied"
}

function nodeClass(value: NodeValue): string {
  if (isCurrentChannel(value)) {
    return "allocation-channel-picker__tree-node--current"
  }
  if (isOccupiedChannel(value)) {
    return isNodeActive(value)
      ? "allocation-channel-picker__tree-node--occupied-active"
      : "allocation-channel-picker__tree-node--occupied"
  }
  if (isNodeActive(value)) {
    return "allocation-channel-picker__tree-node--active"
  }
  return "allocation-channel-picker__tree-node--idle"
}

function nodeCursorClass(value: NodeValue): string {
  if (props.saving) return "allocation-channel-picker__tree-node--cursor-wait"
  if (isUnitNode(value)) return "allocation-channel-picker__tree-node--cursor-pointer"
  if (requiresSwapAction(value) || isCurrentChannel(value)) return "allocation-channel-picker__tree-node--cursor-default"
  return "allocation-channel-picker__tree-node--cursor-pointer"
}

function onNodeClick(value: NodeValue) {
  tree.focus(value)
  if (isUnitNode(value)) {
    tree.toggle(value)
    return
  }
  const channelId = parseChannelId(value)
  if (channelId === null || props.saving) return
  if (requiresSwapAction(value)) return
  tree.clearSelection()
  tree.select(value)
  emit("select", channelId)
}

function onSwapClick(value: NodeValue) {
  if (!requiresSwapAction(value) || props.saving) return
  const channelId = parseChannelId(value)
  if (channelId === null) return
  tree.focus(value)
  tree.clearSelection()
  tree.select(value)
  emit("select", channelId)
}

function focusNextVisibleNode() {
  if (!visibleNodes.value.length) return
  treeDomFocusActive.value = true
  tree.focusNext()
  void nextTick(() => {
    focusNodeElement(tree.state.value.active ?? visibleNodes.value[0]?.value ?? null)
  })
}

function focusPreviousVisibleNode() {
  if (!visibleNodes.value.length) return
  treeDomFocusActive.value = true
  tree.focusPrevious()
  void nextTick(() => {
    focusNodeElement(tree.state.value.active ?? visibleNodes.value[visibleNodes.value.length - 1]?.value ?? null)
  })
}

function selectActiveNode() {
  const active = tree.state.value.active
  if (!active) return
  onNodeClick(active)
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
    case "ArrowRight":
      if (!isUnitNode(value)) return
      event.preventDefault()
      event.stopPropagation()
      if (!tree.isExpanded(value)) {
        tree.expand(value)
        return
      }
      tree.focus(childrenByParent.value.get(value)?.[0] ?? value)
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
    default:
      return
  }
}

function onPanelKeydownCapture(event: KeyboardEvent) {
  if (event.key !== "Escape" || props.saving) {
    return
  }
  event.preventDefault()
  event.stopPropagation()
  emit("close")
}
</script>

<style scoped>
.allocation-channel-picker {
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 0;
}

.allocation-channel-picker__header {
  border-bottom: 1px solid var(--color-neutral-200);
  padding: 1rem;
}

.allocation-channel-picker__eyebrow {
  color: var(--color-neutral-500);
  font-size: 11px;
  font-weight: 600;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.allocation-channel-picker__current {
  background: var(--color-neutral-50);
  border: 1px solid var(--color-neutral-200);
  border-radius: 1rem;
  margin-top: 1rem;
  padding: 0.75rem;
}

.allocation-channel-picker__current-label {
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 500;
  margin-top: 0.25rem;
}

.allocation-channel-picker__search {
  display: block;
  margin-top: 1rem;
}

.allocation-channel-picker__search-label {
  color: var(--color-neutral-600);
  display: block;
  font-size: var(--text-xs);
  font-weight: 500;
  margin-bottom: 0.25rem;
}

.allocation-channel-picker__search-input {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-300);
  border-radius: 0.75rem;
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  outline: none;
  padding: 0.5rem 0.75rem;
  transition: border-color 0.15s, background-color 0.15s, color 0.15s;
  width: 100%;
}

.allocation-channel-picker__search-input:focus {
  border-color: var(--color-neutral-400);
}

.allocation-channel-picker__summary-row {
  align-items: center;
  color: var(--color-neutral-500);
  display: flex;
  font-size: var(--text-xs);
  gap: 0.75rem;
  justify-content: space-between;
  margin-top: 0.75rem;
}

.allocation-channel-picker__clear-button {
  flex-shrink: 0;
}

.allocation-channel-picker__body {
  flex: 1 1 auto;
  min-height: 0;
  overflow-y: auto;
  padding: 0.75rem;
}

.allocation-channel-picker__empty {
  border: 1px dashed var(--color-neutral-300);
  border-radius: 1rem;
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
  padding: 1.5rem 1rem;
}

.allocation-channel-picker__error {
  background: color-mix(in srgb, var(--color-rose-300) 14%, var(--color-white));
  border: 1px solid color-mix(in srgb, var(--color-rose-300) 70%, var(--color-white));
  border-radius: 1rem;
  color: var(--color-rose-700);
  font-size: var(--text-sm);
  padding: 0.75rem 1rem;
}

.allocation-channel-picker__tree {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: 1rem;
  padding: 0.5rem;
}

.allocation-channel-picker__tree-node {
  align-items: center;
  border-radius: 0.75rem;
  display: flex;
  gap: 0.5rem;
  padding: 0.5rem 0.625rem;
  text-align: left;
  transition: background-color 0.15s, box-shadow 0.15s;
  user-select: none;
  width: 100%;
}

.allocation-channel-picker__tree-node:focus {
  outline: 2px solid color-mix(in srgb, var(--color-blue-500) 45%, transparent);
  outline-offset: 1px;
}

.allocation-channel-picker__tree-node--current {
  background: color-mix(in srgb, var(--color-sky-500) 10%, var(--color-white));
  box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--color-sky-500) 30%, transparent);
}

.allocation-channel-picker__tree-node--occupied-active {
  background: var(--color-amber-50);
  box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--color-amber-300) 70%, transparent);
}

.allocation-channel-picker__tree-node--occupied:hover {
  background: var(--color-amber-50);
}

.allocation-channel-picker__tree-node--active {
  background: var(--color-neutral-100);
}

.allocation-channel-picker__tree-node--idle:hover {
  background: var(--color-neutral-50);
}

.allocation-channel-picker__tree-node--cursor-wait {
  cursor: wait;
}

.allocation-channel-picker__tree-node--cursor-pointer {
  cursor: pointer;
}

.allocation-channel-picker__tree-node--cursor-default {
  cursor: default;
}

.allocation-channel-picker__indent {
  flex-shrink: 0;
}

.allocation-channel-picker__chevron {
  color: var(--color-neutral-500);
  flex-shrink: 0;
  font-size: 10px;
  transition: transform 0.15s;
}

.allocation-channel-picker__chevron--expanded {
  transform: rotate(90deg);
}

.allocation-channel-picker__spinner {
  animation: allocation-channel-picker-spin 1s linear infinite;
  border: 2px solid var(--color-sky-500);
  border-radius: 999px;
  border-top-color: transparent;
  flex-shrink: 0;
  height: 0.75rem;
  width: 0.75rem;
}

.allocation-channel-picker__channel-dot {
  border-radius: 999px;
  flex-shrink: 0;
  height: 0.375rem;
  width: 0.375rem;
}

.allocation-channel-picker__unit-dot {
  border-radius: 999px;
  flex-shrink: 0;
  height: 0.5rem;
  width: 0.5rem;
}

.allocation-channel-picker__channel-dot--unknown,
.allocation-channel-picker__unit-dot--unknown {
  background: var(--color-neutral-400);
}

.allocation-channel-picker__channel-dot--occupied,
.allocation-channel-picker__unit-dot--occupied {
  background: var(--color-amber-500);
}

.allocation-channel-picker__channel-dot--free,
.allocation-channel-picker__unit-dot--free {
  background: var(--color-emerald-500);
}

.allocation-channel-picker__node-label {
  cursor: inherit;
  font-size: var(--text-sm);
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.allocation-channel-picker__node-label--unit {
  color: var(--color-neutral-900);
  font-weight: 600;
}

.allocation-channel-picker__node-label--channel {
  color: var(--color-neutral-800);
}

.allocation-channel-picker__node-label--grow {
  flex: 1 1 auto;
}

.allocation-channel-picker__node-label--with-owner {
  flex-shrink: 0;
  max-width: 7rem;
}

.allocation-channel-picker__owner {
  background: color-mix(in srgb, var(--color-amber-50) 70%, transparent);
  border: 1px solid color-mix(in srgb, var(--color-amber-300) 60%, transparent);
  border-radius: var(--radius-lg);
  color: var(--color-amber-900);
  cursor: inherit;
  flex: 1 1 auto;
  font-size: 10px;
  min-width: 0;
  overflow: hidden;
  padding: 0.25rem 0.5rem;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.allocation-channel-picker__tag {
  flex-shrink: 0;
  font-size: 10px;
  font-weight: 600;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.allocation-channel-picker__tag--current {
  color: var(--color-neutral-500);
}

.allocation-channel-picker__swap-button {
  flex-shrink: 0;
}

:global(.dark .allocation-channel-picker__header) {
  border-bottom-color: var(--color-neutral-700);
}

:global(.dark .allocation-channel-picker__eyebrow),
:global(.dark .allocation-channel-picker__summary-row),
:global(.dark .allocation-channel-picker__empty),
:global(.dark .allocation-channel-picker__chevron),
:global(.dark .allocation-channel-picker__tag--current) {
  color: var(--color-neutral-400);
}

:global(.dark .allocation-channel-picker__current-label),
:global(.dark .allocation-channel-picker__node-label--unit),
:global(.dark .allocation-channel-picker__search-input) {
  color: var(--color-neutral-100);
}

:global(.dark .allocation-channel-picker__current),
:global(.dark .allocation-channel-picker__tree) {
  background: color-mix(in srgb, var(--color-neutral-900) 70%, transparent);
  border-color: var(--color-neutral-700);
}

:global(.dark .allocation-channel-picker__search-label) {
  color: var(--color-neutral-300);
}

:global(.dark .allocation-channel-picker__search-input) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
}

:global(.dark .allocation-channel-picker__search-input:focus) {
  border-color: var(--color-neutral-500);
}

:global(.dark .allocation-channel-picker__empty) {
  border-color: var(--color-neutral-700);
}

:global(.dark .allocation-channel-picker__error) {
  background: color-mix(in srgb, var(--color-rose-700) 30%, var(--color-neutral-950));
  border-color: color-mix(in srgb, var(--color-rose-700) 60%, var(--color-neutral-950));
  color: var(--color-rose-300);
}

:global(.dark .allocation-channel-picker__tree-node--current) {
  background: color-mix(in srgb, var(--color-sky-500) 20%, var(--color-neutral-950));
  box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--color-sky-500) 60%, transparent);
}

:global(.dark .allocation-channel-picker__tree-node--occupied-active) {
  background: color-mix(in srgb, var(--color-amber-900) 40%, transparent);
  box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--color-amber-700) 70%, transparent);
}

:global(.dark .allocation-channel-picker__tree-node--occupied:hover) {
  background: color-mix(in srgb, var(--color-amber-900) 30%, transparent);
}

:global(.dark .allocation-channel-picker__tree-node--active) {
  background: color-mix(in srgb, var(--color-neutral-800) 70%, transparent);
}

:global(.dark .allocation-channel-picker__tree-node--idle:hover) {
  background: color-mix(in srgb, var(--color-neutral-800) 60%, transparent);
}

:global(.dark .allocation-channel-picker__channel-dot--unknown),
:global(.dark .allocation-channel-picker__unit-dot--unknown) {
  background: var(--color-neutral-600);
}

:global(.dark .allocation-channel-picker__node-label--channel) {
  color: var(--color-neutral-200);
}

:global(.dark .allocation-channel-picker__owner) {
  background: color-mix(in srgb, var(--color-amber-900) 30%, transparent);
  border-color: color-mix(in srgb, var(--color-amber-900) 80%, transparent);
  color: var(--color-amber-50);
}

@keyframes allocation-channel-picker-spin {
  to {
    transform: rotate(360deg);
  }
}
</style>
