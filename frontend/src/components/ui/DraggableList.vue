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
      :ref="getItemRefHandler(item)"
    >
      <!-- BEFORE indicator -->
      <div
        v-if="indicatorVisible(index, 'before')"
        class="draggable-list__indicator draggable-list__indicator--before"
      ></div>

      <!-- CONTENT -->
      <div class="draggable-list__content">
        <!--
          Пользовательский слот.
          Можно использовать drag-handle внутри, мы даём флаги и индекс.
        -->
        <slot
          :item="item"
          :index="index"
          :isDragging="draggingIndex === index"
          :isKeyboardDragging="keyboardDragIndex === index"
        />
      </div>

      <!-- AFTER indicator -->
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

/* ------------------------------------------------------------------ */
/* STATE                                                              */
/* ------------------------------------------------------------------ */

const draggingIndex = ref<number | null>(null)
const dragOverState = ref<{ index: number; position: "before" | "after" } | null>(null)
const keyboardDragIndex = ref<number | null>(null)
const activeIndex = ref<number>(-1)

const idPrefix = `draggable-${Math.random().toString(36).slice(2)}`

const axis = computed(() => props.axis ?? "vertical")
const wrapperTag = computed(
  () => props.wrapperTag ?? "ul",
)
const itemTag = computed(
  () =>
    props.itemTag ??
    (wrapperTag.value === "ul" || wrapperTag.value === "ol" ? "li" : "div"),
)
const isListSemantic = computed(
  () => wrapperTag.value === "ul" || wrapperTag.value === "ol",
)
const wrapperRole = computed(() => (isListSemantic.value ? "listbox" : undefined))
const itemRole = computed(() => (isListSemantic.value ? "option" : undefined))

const activeDescendantId = computed(() =>
  activeIndex.value >= 0 ? itemId(activeIndex.value) : undefined,
)

/* ------------------------------------------------------------------ */
/* DOM refs (для FLIP + фокуса)                                       */
/* ------------------------------------------------------------------ */

const itemRefs = ref<Map<string, HTMLElement>>(new Map())
const itemRefHandlers = new Map<string, (el: Element | null) => void>()
const lastRects = ref<Record<string, DOMRect>>({})

function setItemRefByKey(key: string, el: Element | null) {
  const map = itemRefs.value
  if (el) {
    map.set(key, el as HTMLElement)
  } else {
    map.delete(key)
  }
}

function getItemRefHandler(item: T) {
  const key = props.itemKey(item)
  let handler = itemRefHandlers.get(key)
  if (!handler) {
    handler = (el: Element | null) => setItemRefByKey(key, el)
    itemRefHandlers.set(key, handler)
  }
  return handler
}

function captureRectsBeforeReorder() {
  const rects: Record<string, DOMRect> = {}
  for (const [key, el] of itemRefs.value.entries()) {
    rects[key] = el.getBoundingClientRect()
  }
  lastRects.value = rects
}

async function animateFlip() {
  const prev = lastRects.value
  if (!prev || Object.keys(prev).length === 0) return

  await nextTick()

  for (const [key, el] of itemRefs.value.entries()) {
    const oldRect = prev[key]
    if (!oldRect) continue
    const newRect = el.getBoundingClientRect()

    const dx = oldRect.left - newRect.left
    const dy = oldRect.top - newRect.top

    if (dx === 0 && dy === 0) continue

    el.style.transition = "none"
    el.style.transform = `translate(${dx}px, ${dy}px)`

    requestAnimationFrame(() => {
      el.style.transition = "transform 150ms ease"
      el.style.transform = ""
    })
  }

  lastRects.value = {}
}

/* ------------------------------------------------------------------ */
/* HELPERS                                                            */
/* ------------------------------------------------------------------ */

const items = computed(() => props.items)

function itemId(index: number) {
  return `${idPrefix}-${index}`
}

function itemStyleAttr(index: number) {
  const resolver = props.itemStyle
  if (!resolver) return undefined
  const target = items.value[index]
  if (target === undefined) return undefined
  return resolver(target, index)
}

function isItemDraggable(index: number) {
  const predicate = props.itemDraggable
  if (!predicate) return true
  const target = items.value[index]
  if (target === undefined) return true
  try {
    return predicate(target, index)
  } catch (error) {
    console.warn("DraggableList itemDraggable predicate threw", error)
    return false
  }
}

function itemClass(index: number) {
  return {
    "draggable-list__item--dragging":
      draggingIndex.value === index || keyboardDragIndex.value === index,
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

/**
 * Реальный реордер со FLIP-анимацией
 */
function reorder(from: number, to: number): number | null {
  if (from === to) return null
  const list = [...items.value]
  if (from < 0 || from >= list.length) return null

  // 1) Снять старые позиции
  captureRectsBeforeReorder()

  const [moved] = list.splice(from, 1)
  const target = Math.max(0, Math.min(list.length, to))
  list.splice(target, 0, moved)

  // 2) Эмитим новое состояние
  emitReordered(list)

  // 3) Анимируем переход
  animateFlip()

  return Math.max(0, Math.min(list.length - 1, target))
}

/* ------------------------------------------------------------------ */
/* MOUSE / POINTER DnD                                                */
/* ------------------------------------------------------------------ */

function handleDragStart(index: number, event: DragEvent) {
  if (!event.dataTransfer) return
  if (!isItemDraggable(index)) {
    event.preventDefault()
    return
  }
  draggingIndex.value = index
  dragOverState.value = { index, position: "before" }

  event.dataTransfer.effectAllowed = "move"
  // Firefox: обязательно нужно что-то записать
  event.dataTransfer.setData(
    "text/plain",
    props.itemKey(items.value[index]),
  )
}

function handleDragEnter(index: number, _event: DragEvent) {
  if (draggingIndex.value === null) return
  if (!isItemDraggable(draggingIndex.value)) return
  if (index === draggingIndex.value) return

  dragOverState.value = { index, position: "before" }
}

function handleDragOver(index: number, event: DragEvent) {
  if (draggingIndex.value === null) return
  if (!isItemDraggable(draggingIndex.value)) return

  const target = event.currentTarget as HTMLElement | null
  if (!target) return

  const rect = target.getBoundingClientRect()
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
  const state = dragOverState.value
  if (state && state.index === index) {
    dragOverState.value = null
  }
}

function handleDrop(index: number) {
  if (draggingIndex.value === null) return
  if (!isItemDraggable(draggingIndex.value)) {
    draggingIndex.value = null
    dragOverState.value = null
    return
  }

  const dropState = dragOverState.value ?? {
    index,
    position: "before" as const,
  }

  let targetIndex =
    dropState.position === "before" ? dropState.index : dropState.index + 1
  if (targetIndex < 0) targetIndex = 0
  if (targetIndex > items.value.length) targetIndex = items.value.length

  const resultIndex = reorder(draggingIndex.value, targetIndex)

  draggingIndex.value = null
  dragOverState.value = null

  if (resultIndex !== null) {
    nextTick(() => {
      const ref = document.getElementById(itemId(resultIndex))
      ref?.focus()
      activeIndex.value = resultIndex
    })
  }
}

function handleDragEnd() {
  draggingIndex.value = null
  dragOverState.value = null
}

/* ------------------------------------------------------------------ */
/* KEYBOARD DnD                                                       */
/* ------------------------------------------------------------------ */

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
  if (to >= items.value.length) to = items.value.length - 1

  if (to !== from) {
    reorder(from, to)
    keyboardDragIndex.value = to
    activeIndex.value = to
  }
}

function focusRelative(offset: number, event: KeyboardEvent) {
  event.preventDefault()
  let next = (activeIndex.value >= 0 ? activeIndex.value : 0) + offset
  next = Math.max(0, Math.min(items.value.length - 1, next))
  const el = document.getElementById(itemId(next))
  el?.focus()
}

function handleKeydown(index: number, event: KeyboardEvent) {
  const { key } = event

  // Навигация по списку
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

  // Вкл/выкл keyboard-drag режима
  if (key === "Enter" || key === " ") {
    event.preventDefault()
    if (!isItemDraggable(index)) return

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

/* ------------------------------------------------------------------ */
/* WATCHES                                                            */
/* ------------------------------------------------------------------ */

watch(
  () => items.value,
  () => {
    // Сброс локальных состояний при внешней смене items
    if (activeIndex.value >= items.value.length) {
      activeIndex.value = items.value.length - 1
    }
    draggingIndex.value = null
    dragOverState.value = null
    keyboardDragIndex.value = null
  },
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
  transition:
    background-color 0.15s ease,
    box-shadow 0.15s ease;
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

/* Indicators ------------------------------------------------------- */

.draggable-list__indicator {
  position: absolute;
  background: rgba(59, 130, 246, 0.85);
  pointer-events: none;
  border-radius: 9999px;
}

.draggable-list[data-axis="vertical"]
  .draggable-list__indicator--before {
  top: -2px;
  left: 0;
  right: 0;
  height: 3px;
}

.draggable-list[data-axis="vertical"]
  .draggable-list__indicator--after {
  bottom: -2px;
  left: 0;
  right: 0;
  height: 3px;
}

.draggable-list[data-axis="horizontal"]
  .draggable-list__indicator--before {
  top: 0;
  bottom: 0;
  left: -2px;
  width: 3px;
}

.draggable-list[data-axis="horizontal"]
  .draggable-list__indicator--after {
  top: 0;
  bottom: 0;
  right: -2px;
  width: 3px;
}
</style>
