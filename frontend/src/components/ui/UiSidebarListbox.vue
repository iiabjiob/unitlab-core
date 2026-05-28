<script setup lang="ts">
import { computed, nextTick, ref, watch } from "vue"
import type { ComponentPublicInstance } from "vue"

type SidebarItem = any

const props = withDefaults(defineProps<{
  items: SidebarItem[]
  activeId?: string | number | null
  selectedIds?: Array<string | number>
  idKey?: string
  ariaLabel?: string
  disabled?: boolean
}>(), {
  activeId: null,
  selectedIds: () => [],
  idKey: "id",
  ariaLabel: "Sidebar list",
  disabled: false,
})

const emit = defineEmits<{
  (e: "select", id: string | number, event?: MouseEvent | KeyboardEvent): void
  (e: "delete"): void
}>()

const listboxRef = ref<HTMLDivElement | null>(null)
const optionRefs = new Map<number, HTMLElement>()
const cursorIndex = ref<number>(-1)

const normalizedItems = computed(() => props.items ?? [])
const selectedIdSet = computed(() => new Set(props.selectedIds))

const activeDescendantId = computed(() => {
  if (cursorIndex.value < 0) return null
  const target = normalizedItems.value[cursorIndex.value]
  if (!target) return null
  const id = resolveItemId(target)
  if (id === null) return null
  return optionId(id)
})

watch(
  () => [normalizedItems.value.length, props.activeId] as const,
  () => {
    const activeIdx = normalizedItems.value.findIndex(item => resolveItemId(item) === props.activeId)
    if (activeIdx >= 0) {
      cursorIndex.value = activeIdx
      return
    }
    cursorIndex.value = normalizedItems.value.length ? 0 : -1
  },
  { immediate: true },
)

watch(
  () => cursorIndex.value,
  (next) => {
    if (next < 0) return
    nextTick(() => {
      optionRefs.get(next)?.scrollIntoView({ block: "nearest" })
    })
  },
)

function resolveItemId(item: SidebarItem): string | number | null {
  const candidate = item?.[props.idKey]
  if (typeof candidate === "string" || typeof candidate === "number") {
    return candidate
  }
  return null
}

function optionId(id: string | number): string {
  return `sidebar-listbox-option-${String(id).replace(/[^a-zA-Z0-9_-]/g, "-")}`
}

function setOptionRef(index: number, el: Element | ComponentPublicInstance | null) {
  const target = el instanceof HTMLElement
    ? el
    : el && "$el" in el && el.$el instanceof HTMLElement
      ? el.$el
      : null

  if (target) {
    optionRefs.set(index, target)
    return
  }
  optionRefs.delete(index)
}

function isCursor(index: number): boolean {
  return index === cursorIndex.value
}

function isActive(item: SidebarItem): boolean {
  return resolveItemId(item) === props.activeId
}

function isSelected(item: SidebarItem): boolean {
  const id = resolveItemId(item)
  return id !== null && selectedIdSet.value.has(id)
}

function moveCursor(delta: number) {
  const total = normalizedItems.value.length
  if (!total) {
    cursorIndex.value = -1
    return
  }
  const base = cursorIndex.value < 0 ? 0 : cursorIndex.value
  cursorIndex.value = Math.max(0, Math.min(total - 1, base + delta))
}

function activateCursor(event?: KeyboardEvent) {
  if (props.disabled) return
  const target = normalizedItems.value[cursorIndex.value]
  if (!target) return
  const id = resolveItemId(target)
  if (id === null) return
  emit("select", id, event)
}

function focusListbox() {
  listboxRef.value?.focus({ preventScroll: true })
}

function handleFocus() {
  if (props.disabled) return
  if (cursorIndex.value >= 0) return
  cursorIndex.value = normalizedItems.value.length ? 0 : -1
}

function handleKeydown(event: KeyboardEvent) {
  if (props.disabled) return
  switch (event.key) {
    case "ArrowDown":
      event.preventDefault()
      moveCursor(1)
      return
    case "ArrowUp":
      event.preventDefault()
      moveCursor(-1)
      return
    case "Home":
      event.preventDefault()
      cursorIndex.value = normalizedItems.value.length ? 0 : -1
      return
    case "End":
      event.preventDefault()
      cursorIndex.value = normalizedItems.value.length ? normalizedItems.value.length - 1 : -1
      return
    case "Enter":
    case " ":
      event.preventDefault()
      activateCursor(event)
      return
    case "Delete":
    case "Backspace":
      event.preventDefault()
      emit("delete")
      return
    default:
      return
  }
}

function handleItemPointerDown(index: number) {
  if (props.disabled) return
  cursorIndex.value = index
  focusListbox()
}

function handleItemClick(index: number, event: MouseEvent) {
  if (props.disabled) return
  const target = normalizedItems.value[index]
  if (!target) return
  const id = resolveItemId(target)
  if (id === null) return
  cursorIndex.value = index
  emit("select", id, event)
}
</script>

<template>
  <div
    ref="listboxRef"
    class="ui-sidebar-listbox"
    :tabindex="disabled ? -1 : 0"
    role="listbox"
    :aria-label="ariaLabel"
    :aria-activedescendant="activeDescendantId ?? undefined"
    :aria-multiselectable="selectedIds.length > 0 ? 'true' : undefined"
    @focus="handleFocus"
    @keydown="handleKeydown"
  >
    <div
      v-for="(item, index) in normalizedItems"
      :id="resolveItemId(item) !== null ? optionId(resolveItemId(item) as string | number) : undefined"
      :key="resolveItemId(item) ?? index"
      :ref="(el) => setOptionRef(index, el)"
      role="option"
      :aria-selected="isActive(item) || isSelected(item)"
      class="ui-sidebar-listbox__option"
      @pointerdown="handleItemPointerDown(index)"
      @click="handleItemClick(index, $event)"
    >
      <slot
        name="item"
        :item="item"
        :index="index"
        :is-active="isActive(item)"
        :is-selected="isSelected(item)"
        :is-cursor="isCursor(index)"
      />
    </div>

    <slot v-if="!normalizedItems.length" name="empty" />
  </div>
</template>

<style scoped>
.ui-sidebar-listbox {
  border-radius: 0.5rem;
  overflow-y: auto;
}

.ui-sidebar-listbox:focus {
  outline: none;
}

.ui-sidebar-listbox__option {
  border-radius: 0.5rem;
}

.ui-sidebar-listbox__option + .ui-sidebar-listbox__option {
  margin-top: 0.25rem;
}
</style>
