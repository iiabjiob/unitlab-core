<template>
  <component
    :is="wrapperTag"
    class="draggable-list select-none"
    :data-axis="axis"
    :role="wrapperRole"
    :aria-activedescendant="activeDescendantId"
  >
    <component
      v-for="(item, index) in items"
      :is="itemTag"
      :id="itemId(index)"
      :key="itemKey(item)"
      class="draggable-list__item"
      :class="itemClass(index)"
      :style="itemStyleAttr(index)"
      tabindex="0"
      :role="itemRole"
      :draggable="isItemDraggable(index)"
      @dragstart="handleDragStart(index, $event)"
      @dragover.prevent="handleDragOver(index, $event)"
      @dragenter.prevent="handleDragEnter(index, $event)"
      @dragleave="handleDragLeave(index)"
      @drop.prevent="handleDrop(index)"
      @dragend="handleDragEnd"
      @keydown="handleKeydown(index, $event)"
      @focus="handleFocus(index)"
    >
      <div
        v-if="indicatorVisible(index, 'before')"
        class="draggable-list__indicator draggable-list__indicator--before"
      ></div>
      <div class="draggable-list__content">
        <slot :item="item" :index="index" />
      </div>
      <div
        v-if="indicatorVisible(index, 'after')"
        class="draggable-list__indicator draggable-list__indicator--after"
      ></div>
    </component>
  </component>
</template>

<script setup lang="ts" generic="T">
import { computed, nextTick, ref, watch } from "vue"

export interface DraggableListProps<T> {
  items: T[]
  itemKey: (item: T) => string
  axis?: "vertical" | "horizontal"
  wrapperTag?: keyof HTMLElementTagNameMap | string
  itemTag?: keyof HTMLElementTagNameMap | string
  itemStyle?: (item: T, index: number) => Record<string, string | number> | undefined
  itemDraggable?: (item: T, index: number) => boolean
}

const props = defineProps<DraggableListProps<T>>()

const emit = defineEmits<{
  (e: "update:items", items: T[]): void
}>()

const draggingIndex = ref<number | null>(null)
const dragOverState = ref<{ index: number; position: "before" | "after" } | null>(null)
const currentRect = ref<DOMRect | null>(null)
const keyboardDragIndex = ref<number | null>(null)
const activeIndex = ref<number>(-1)
const idPrefix = `draggable-${Math.random().toString(36).slice(2)}`

const activeDescendantId = computed(() => (activeIndex.value >= 0 ? itemId(activeIndex.value) : undefined))
const axis = computed(() => props.axis ?? "vertical")
const wrapperTag = computed(() => props.wrapperTag ?? "ul")
const itemTag = computed(() => props.itemTag ?? (wrapperTag.value === "ul" || wrapperTag.value === "ol" ? "li" : "div"))
const isListSemantic = computed(() => wrapperTag.value === "ul" || wrapperTag.value === "ol")
const wrapperRole = computed(() => (isListSemantic.value ? "listbox" : undefined))
const itemRole = computed(() => (isListSemantic.value ? "option" : undefined))

function itemStyleAttr(index: number) {
  const resolver = props.itemStyle
  if (!resolver) return undefined
  const target = props.items[index]
  if (target === undefined) return undefined
  return resolver(target, index)
}

function isItemDraggable(index: number) {
  const predicate = props.itemDraggable
  if (!predicate) return true
  const target = props.items[index]
  if (target === undefined) return true
  try {
    return predicate(target, index)
  } catch (error) {
    console.warn("DraggableList itemDraggable predicate threw", error)
    return false
  }
}

function itemId(index: number) {
  return `${idPrefix}-${index}`
}

function itemClass(index: number) {
  return {
    "draggable-list__item--dragging": draggingIndex.value === index || keyboardDragIndex.value === index,
    "draggable-list__item--keyboard": keyboardDragIndex.value === index,
    "draggable-list__item--disabled": !isItemDraggable(index),
  }
}

function indicatorVisible(index: number, position: "before" | "after") {
  const state = dragOverState.value
  if (!state) return false
  if (state.index !== index) return false
  return state.position === position
}

function emitReordered(nextItems: T[]) {
  emit("update:items", nextItems)
}

function reorder(from: number, to: number): number | null {
  if (from === to) return null
  const items = [...props.items]
  const [moved] = items.splice(from, 1)
  const target = Math.max(0, Math.min(items.length, to))
  items.splice(target, 0, moved)
  emitReordered(items)
  return Math.max(0, Math.min(items.length - 1, target))
}

function handleDragStart(index: number, event: DragEvent) {
  if (!event.dataTransfer) return
  if (!isItemDraggable(index)) {
    event.preventDefault()
    return
  }
  draggingIndex.value = index
  dragOverState.value = { index, position: "before" }
  currentRect.value = (event.currentTarget as HTMLElement | null)?.getBoundingClientRect() ?? null
  event.dataTransfer.effectAllowed = "move"
  // Firefox requires dataTransfer data to be set
  event.dataTransfer.setData("text/plain", props.itemKey(props.items[index]))
}

function handleDragEnter(index: number, event: DragEvent) {
  if (draggingIndex.value === null) return
  if (!isItemDraggable(draggingIndex.value)) {
    return
  }
  if (index === draggingIndex.value) return
  dragOverState.value = { index, position: "before" }
  currentRect.value = (event.currentTarget as HTMLElement | null)?.getBoundingClientRect() ?? null
}

function handleDragOver(index: number, event: DragEvent) {
  if (draggingIndex.value === null) return
  if (!isItemDraggable(draggingIndex.value)) {
    return
  }
  if (!currentRect.value) {
    currentRect.value = (event.currentTarget as HTMLElement | null)?.getBoundingClientRect() ?? null
    if (!currentRect.value) return
  }

  const rect = currentRect.value
  const halfway =
    axis.value === "horizontal"
      ? rect.left + rect.width / 2
      : rect.top + rect.height / 2
  const position: "before" | "after" =
    axis.value === "horizontal"
      ? event.clientX > halfway
        ? "after"
        : "before"
      : event.clientY > halfway
        ? "after"
        : "before"
  dragOverState.value = { index, position }
}

function handleDragLeave(index: number) {
  if (dragOverState.value && dragOverState.value.index === index) {
    dragOverState.value = null
  }
  currentRect.value = null
}

function handleDrop(index: number) {
  if (draggingIndex.value === null) return
  if (!isItemDraggable(draggingIndex.value)) {
    draggingIndex.value = null
    dragOverState.value = null
    currentRect.value = null
    return
  }
  const dropState = dragOverState.value ?? { index, position: "before" as const }
  let targetIndex = dropState.position === "before" ? dropState.index : dropState.index + 1
  if (targetIndex < 0) targetIndex = 0
  const resultIndex = reorder(draggingIndex.value, targetIndex)
  draggingIndex.value = null
  dragOverState.value = null
  currentRect.value = null
  if (resultIndex !== null) {
    nextTick(() => {
      document.getElementById(itemId(resultIndex))?.focus()
      activeIndex.value = resultIndex
    })
  }
}

function handleDragEnd() {
  draggingIndex.value = null
  dragOverState.value = null
  currentRect.value = null
  if (activeIndex.value >= 0) {
    nextTick(() => {
      document.getElementById(itemId(activeIndex.value))?.focus()
    })
  }
}

function handleFocus(index: number) {
  activeIndex.value = index
}

function beginKeyboardDrag(index: number) {
  keyboardDragIndex.value = index
  activeIndex.value = index
}

function endKeyboardDrag() {
  keyboardDragIndex.value = null
}

function moveKeyboardDrag(delta: number) {
  if (keyboardDragIndex.value === null) return
  const from = keyboardDragIndex.value
  let to = from + delta
  if (to < 0) to = 0
  if (to >= props.items.length) to = props.items.length - 1
  if (to !== from) {
    reorder(from, to)
    keyboardDragIndex.value = to
    activeIndex.value = to
  }
}

function focusRelative(offset: number, event: KeyboardEvent) {
  event.preventDefault()
  let next = (activeIndex.value >= 0 ? activeIndex.value : 0) + offset
  next = Math.max(0, Math.min(props.items.length - 1, next))
  const nextElement = document.getElementById(itemId(next))
  nextElement?.focus()
}

function handleKeydown(index: number, event: KeyboardEvent) {
  const { key } = event

  if (key === "ArrowUp" || key === "ArrowLeft") {
    if (keyboardDragIndex.value !== null) {
      event.preventDefault()
      moveKeyboardDrag(-1)
    } else {
      focusRelative(-1, event)
    }
    return
  }

  if (key === "ArrowDown" || key === "ArrowRight") {
    if (keyboardDragIndex.value !== null) {
      event.preventDefault()
      moveKeyboardDrag(1)
    } else {
      focusRelative(1, event)
    }
    return
  }

  if (key === "Enter" || key === " ") {
    event.preventDefault()
    if (!isItemDraggable(index)) {
      return
    }
    if (keyboardDragIndex.value === null) {
      beginKeyboardDrag(index)
    } else {
      endKeyboardDrag()
    }
    return
  }

  if (key === "Escape") {
    if (keyboardDragIndex.value !== null) {
      event.preventDefault()
      endKeyboardDrag()
    }
  }
}

watch(
  () => props.items,
  () => {
    draggingIndex.value = null
    dragOverState.value = null
    keyboardDragIndex.value = null
    currentRect.value = null
    if (activeIndex.value >= props.items.length) {
      activeIndex.value = props.items.length - 1
    }
  }
)
</script>

<style scoped>
.draggable-list {
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
}

.draggable-list[data-axis="horizontal"] {
  display: contents;
}

.draggable-list__item {
  cursor: grab;
  outline: none;
  transition: background-color 0.15s ease, box-shadow 0.15s ease;
  position: relative;
  display: flex;
  align-items: center;
}

.draggable-list__item--disabled {
  cursor: default;
}

.draggable-list__item--disabled:focus-visible {
  box-shadow: none;
}

.draggable-list[data-axis="horizontal"] .draggable-list__item {
  display: block;
  padding: 0;
  height: 100%;
}

.draggable-list__content {
  width: 100%;
  display: block;
  min-width: 0;
}

.draggable-list[data-axis="horizontal"] .draggable-list__content {
  display: block;
  height: 100%;
}

.draggable-list__item:focus-visible {
  box-shadow: 0 0 0 2px rgba(59, 130, 246, 0.5);
}

.draggable-list__item--dragging {
  opacity: 0.6;
  cursor: grabbing;
}

.draggable-list__item--keyboard {
  box-shadow: inset 0 0 0 2px rgba(59, 130, 246, 0.35);
}

.draggable-list__indicator {
  position: absolute;
  background: rgba(59, 130, 246, 0.85);
  pointer-events: none;
  border-radius: 9999px;
}

.draggable-list[data-axis="vertical"] .draggable-list__indicator--before {
  top: -2px;
  left: 0;
  right: 0;
  height: 3px;
}

.draggable-list[data-axis="vertical"] .draggable-list__indicator--after {
  bottom: -2px;
  left: 0;
  right: 0;
  height: 3px;
}

.draggable-list[data-axis="horizontal"] .draggable-list__indicator--before {
  top: 0;
  bottom: 0;
  left: -2px;
  width: 3px;
}

.draggable-list[data-axis="horizontal"] .draggable-list__indicator--after {
  top: 0;
  bottom: 0;
  right: -2px;
  width: 3px;
}
</style>
