<template>
  <SlideOver
    :open="open"
    placement="right"
    title="Assign unit/channel"
    :width-px="560"
    :close-on-backdrop="!saving"
    @close="emit('close')"
  >
    <div class="flex h-full min-h-0 flex-col" @keydown.capture="onPanelKeydownCapture">
      <div class="border-b border-neutral-200 px-4 py-4 dark:border-neutral-700">
        <div class="text-[11px] font-semibold uppercase tracking-[0.08em] text-neutral-500 dark:text-neutral-400">
          {{ signalDirection }} signal
        </div>
        <div class="mt-1 text-sm font-semibold text-neutral-900 dark:text-neutral-100">
          {{ signalTitle }}
        </div>
        <div v-if="signalKeyText" class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">
          {{ signalKeyText }}
        </div>

        <div class="mt-4 rounded-2xl border border-neutral-200 bg-neutral-50 px-3 py-3 dark:border-neutral-700 dark:bg-neutral-900/70">
          <div class="text-[11px] font-semibold uppercase tracking-[0.08em] text-neutral-500 dark:text-neutral-400">
            Current allocation
          </div>
          <div class="mt-1 text-sm font-medium text-neutral-900 dark:text-neutral-100">
            {{ currentLabel }}
          </div>
        </div>

        <label class="mt-4 block">
          <span class="mb-1 block text-xs font-medium text-neutral-600 dark:text-neutral-300">Search channels</span>
          <input
            ref="searchInputRef"
            v-model="query"
            data-dialog-initial
            type="text"
            autocomplete="off"
            spellcheck="false"
            class="w-full rounded-xl border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 outline-none transition focus:border-neutral-400 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100 dark:focus:border-neutral-500"
            placeholder="Filter by unit, channel, or name"
            :disabled="loading || saving"
            @focus="treeDomFocusActive = false"
            @keydown.down.prevent="focusNextVisibleNode()"
            @keydown.up.prevent="focusPreviousVisibleNode()"
            @keydown.enter.prevent="selectActiveNode()"
          />
        </label>

        <div class="mt-3 flex items-center justify-between gap-3 text-xs text-neutral-500 dark:text-neutral-400">
          <span>{{ resultsSummary }}</span>
          <button
            v-if="currentChannelId !== null"
            type="button"
            class="rounded-lg border border-neutral-300 px-2 py-1 font-medium text-neutral-700 transition hover:bg-neutral-100 disabled:cursor-not-allowed disabled:opacity-60 dark:border-neutral-600 dark:text-neutral-200 dark:hover:bg-neutral-800"
            :disabled="saving"
            @click="emit('select', null)"
          >
            Clear allocation
          </button>
        </div>
      </div>

      <div class="min-h-0 flex-1 overflow-y-auto px-3 py-3">
        <div v-if="loading" class="rounded-2xl border border-dashed border-neutral-300 px-4 py-6 text-sm text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
          Loading channel catalog…
        </div>

        <div v-else-if="error" class="rounded-2xl border border-rose-200 bg-rose-50 px-4 py-3 text-sm text-rose-700 dark:border-rose-900/70 dark:bg-rose-950/30 dark:text-rose-200">
          {{ error }}
        </div>

        <div v-else-if="visibleNodes.length === 0" class="rounded-2xl border border-dashed border-neutral-300 px-4 py-6 text-sm text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
          No compatible channels found.
        </div>

        <div
          v-else
          class="rounded-2xl border border-neutral-200 bg-white p-2 dark:border-neutral-700 dark:bg-neutral-900/70"
          role="tree"
          aria-label="Channel tree"
          @keydown="onTreeRootKeydown"
        >
          <button
            v-for="node in visibleNodes"
            :key="node.value"
            :ref="bindItemElement(node.value)"
            type="button"
            class="flex w-full items-center gap-2 rounded-xl px-2.5 py-2 text-left transition"
            :class="nodeClass(node.value)"
            :aria-level="nodeLevel(node.value)"
            :aria-expanded="isUnitNode(node.value) ? isExpanded(node.value) : undefined"
            :aria-selected="isChannelNode(node.value) ? isNodeSelected(node.value) : undefined"
            role="treeitem"
            :tabindex="isNodeActive(node.value) ? 0 : -1"
            @focus="handleTreeNodeFocus(node.value)"
            @keydown="onNodeKeydown($event, node.value)"
            @click="onNodeClick(node.value)"
          >
            <span class="channel-picker-tree__indent" :style="{ width: `${(nodeLevel(node.value) - 1) * 14}px` }"></span>
            <span
              v-if="isUnitNode(node.value)"
              class="shrink-0 text-[10px] text-neutral-500 transition-transform dark:text-neutral-400"
              :class="isExpanded(node.value) ? 'rotate-90' : ''"
              aria-hidden="true"
            >
              ▶
            </span>
            <span v-else class="h-1.5 w-1.5 shrink-0 rounded-full" :class="channelIndicatorClass(node.value)" aria-hidden="true"></span>
            <span
              v-if="isUnitNode(node.value)"
              class="h-2 w-2 shrink-0 rounded-full"
              :class="unitIndicatorClass(node.value)"
              aria-hidden="true"
            ></span>
            <span
              class="min-w-0 truncate"
              :class="[
                isUnitNode(node.value) ? 'text-sm font-semibold text-neutral-900 dark:text-neutral-100' : 'text-sm text-neutral-800 dark:text-neutral-200',
                channelOwnerRowText(node.value) ? 'max-w-28 shrink-0' : 'flex-1',
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
              v-slot="{ setTriggerRef, getTriggerProps }"
            >
              <span
                :ref="setTriggerRef"
                class="min-w-0 flex-1 truncate rounded-lg border border-amber-200 bg-amber-50/70 px-2 py-1 text-[10px] text-amber-900 dark:border-amber-900/80 dark:bg-amber-950/30 dark:text-amber-100"
                v-bind="getTriggerProps()"
              >
                {{ channelOwnerRowText(node.value) }}
              </span>
            </UiHoverTooltip>
            <span
              v-if="isChannelNode(node.value) && isCurrentChannel(node.value)"
              class="shrink-0 text-[10px] font-semibold uppercase tracking-[0.08em] text-neutral-500 dark:text-neutral-400"
            >
              Current
            </span>
            <span
              v-else-if="isChannelNode(node.value) && isOccupiedChannel(node.value)"
              class="shrink-0 text-[10px] font-semibold uppercase tracking-[0.08em] text-amber-600 dark:text-amber-300"
            >
              Swap
            </span>
          </button>
        </div>
      </div>

      <!-- <div class="border-t border-neutral-200 px-4 py-3 text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
        Search narrows units and channels. Enter selects a channel, arrows navigate the tree.
      </div> -->
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
const itemElements = new Map<NodeValue, HTMLButtonElement>()
const treeDomFocusActive = ref(false)
const pendingCurrentChannelReveal = ref(false)
const tree = useTreeviewController<NodeValue>({
  nodes: [],
  loop: true,
})

const normalizedQuery = computed(() => query.value.trim().toLowerCase())

const signalTitle = computed(() => {
  const signalName = props.signalName.trim()
  if (signalName) return signalName
  const signalKey = props.signalKey.trim()
  if (signalKey) return signalKey
  return "Selected signal"
})

const signalKeyText = computed(() => {
  const signalKey = props.signalKey.trim()
  const signalName = props.signalName.trim()
  if (!signalKey || signalKey === signalName) {
    return ""
  }
  return signalKey
})

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

const unitStatusById = computed(() => {
  const map = new Map<string, boolean>()
  groupedChannels.value.forEach((group) => {
    map.set(group.unitId, group.entries.some(entry => entry.online))
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
    if (resolved instanceof HTMLButtonElement) {
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

function channelOwnerRowText(value: NodeValue): string {
  const channelId = parseChannelId(value)
  if (channelId === null) return ""
  const channel = channelById.value.get(channelId)
  if (!channel?.occupied || !channel.ownerRowText) return ""
  return channel.ownerRowText
}

function channelIndicatorClass(value: NodeValue): string {
  const channelId = parseChannelId(value)
  if (channelId === null) return "bg-neutral-400 dark:bg-neutral-600"
  const channel = channelById.value.get(channelId)
  if (!channel) return "bg-neutral-400 dark:bg-neutral-600"
  if (channel.occupied && !isCurrentChannel(value)) return "bg-amber-500"
  return channel.online ? "bg-emerald-500" : "bg-amber-500"
}

function unitIndicatorClass(value: NodeValue): string {
  const unitId = parseUnitId(value)
  if (!unitId) return "bg-neutral-400 dark:bg-neutral-600"
  return unitStatusById.value.get(unitId) ? "bg-emerald-500" : "bg-amber-500"
}

function nodeClass(value: NodeValue): string {
  if (isCurrentChannel(value)) {
    return "bg-sky-50 ring-1 ring-inset ring-sky-200 dark:bg-sky-900/30 dark:ring-sky-800"
  }
  if (isOccupiedChannel(value)) {
    return isNodeActive(value)
      ? "bg-amber-50 ring-1 ring-inset ring-amber-200 dark:bg-amber-950/40 dark:ring-amber-900"
      : "hover:bg-amber-50 dark:hover:bg-amber-950/30"
  }
  if (isNodeActive(value)) {
    return "bg-neutral-100 dark:bg-neutral-800/70"
  }
  return "hover:bg-neutral-50 dark:hover:bg-neutral-800/60"
}

function onNodeClick(value: NodeValue) {
  tree.focus(value)
  if (isUnitNode(value)) {
    tree.toggle(value)
    return
  }
  const channelId = parseChannelId(value)
  if (channelId === null || props.saving) return
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
