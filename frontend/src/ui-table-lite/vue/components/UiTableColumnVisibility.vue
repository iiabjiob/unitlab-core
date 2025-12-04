<template>
  <Teleport to="body">
    <transition name="fade">
      <div v-if="panelVisible" class="pointer-events-none fixed inset-0 z-[190]">
        <div
          ref="panelRef"
          class="pointer-events-auto fixed flex flex-col w-[320px] max-w-[92vw] rounded-md border border-neutral-200 bg-white text-neutral-900 shadow-xl ring-1 ring-black/5 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100"
          :style="panelStyle"
          data-testid="column-panel"
          @mousedown.stop
        >
          <header
            class="flex cursor-move select-none items-center justify-between border-b border-neutral-200 px-4 py-3 text-sm font-semibold dark:border-neutral-800"
            @mousedown.prevent="startDrag"
          >
            <span>Columns</span>
            <button
              type="button"
              class="text-xs uppercase tracking-wide text-neutral-500 transition hover:text-neutral-800 dark:text-neutral-400 dark:hover:text-neutral-100"
              @click="emitClose"
            >
              Close
            </button>
          </header>

          <div class="flex max-h-full flex-1 flex-col gap-4 px-4 py-4">
            <div class="flex items-center justify-between gap-2 text-[11px] text-neutral-500 dark:text-neutral-400">
              <button
                type="button"
                class="rounded border border-neutral-300 bg-white px-3 py-1 text-[11px] font-semibold text-neutral-600 transition hover:border-blue-500 hover:text-blue-600 dark:border-neutral-600 dark:bg-neutral-900 dark:text-neutral-200 dark:hover:border-blue-400 dark:hover:text-blue-300"
                data-testid="column-visibility-reset"
                @click="handleReset"
              >
                Reset to Default
              </button>
              <span class="font-medium">{{ visibleCount }} / {{ totalCount }} visible</span>
            </div>
            <div class="flex-1 overflow-y-auto pr-1 min-h-0">
              <DraggableList
                :items="internalColumns"
                :item-key="columnKey"
                class="w-full"
                @update:items="handleReorder"
              >
                <template #default="{ item }">
                  <label
                    :data-col-key="item.key"
                    class="flex min-w-0 w-full items-center gap-2 rounded px-2 py-1 text-sm text-neutral-700 transition hover:bg-blue-50 dark:text-neutral-100 dark:hover:bg-neutral-800"
                  >
                    <input
                      type="checkbox"
                      class="h-4 w-4 cursor-pointer accent-blue-500"
                      :checked="item.visible"
                      data-testid="column-visibility-checkbox"
                      :data-col-key="item.key"
                      :name="fieldName(item.key)"
                      @change="event => handleToggle(item.key, Boolean((event.target as HTMLInputElement)?.checked))"
                    />
                    <span class="flex-1 truncate">{{ item.label }}</span>
                  </label>
                </template>
              </DraggableList>
            </div>
          </div>
        </div>
      </div>
    </transition>
  </Teleport>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, reactive, ref, watch } from "vue"
import { createColumnStorageAccessor, type ColumnVisibilitySnapshot } from "../utils/columnStorage"
import type { UiTableColumn } from "../../core/types"
import DraggableList from "@/components/ui/DraggableList.vue"

interface ColumnPanelState {
  key: string
  label: string
  visible: boolean
}

const props = defineProps<{
  columns: UiTableColumn[]
  storageKey: string
}>()

const emit = defineEmits<{
  (e: "update", payload: ColumnPanelState[]): void
  (e: "close"): void
  (e: "reset"): void
}>()

const internalColumns = ref<ColumnPanelState[]>([])
const columnKey = (column: ColumnPanelState) => column.key

const totalCount = computed(() => internalColumns.value.length)
const visibleCount = computed(() => internalColumns.value.filter(column => column.visible).length)
const storageSlug = computed(() => {
  const slug = props.storageKey?.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/(^-|-$)/g, "")
  return slug || "columns"
})

const panelRef = ref<HTMLElement | null>(null)
const panelVisible = ref(true)
const position = reactive({ x: 0, y: 0 })
const dragOffset = reactive({ x: 0, y: 0 })
const dragging = ref(false)
const hasCustomPosition = ref(false)

const panelStyle = computed(() => ({
  transform: `translate3d(${position.x}px, ${position.y}px, 0)`,
  maxHeight: "min(80vh, 520px)",
}))

function fieldName(key: string) {
  return `column-visibility-${storageSlug.value}-${key}`
}

function mapColumns(columns: UiTableColumn[]): ColumnPanelState[] {
  return columns.map(column => ({
    key: column.key,
    label: column.label,
    visible: column.visible !== false,
  }))
}

function persistState(state: ColumnPanelState[]) {
  const storage = createColumnStorageAccessor(props.storageKey)
  const snapshot: ColumnVisibilitySnapshot[] = state.map(column => ({
    key: column.key,
    label: column.label,
    visible: column.visible,
  }))
  storage.save(snapshot)
}

function emitUpdate(options: { persist?: boolean } = {}) {
  const persist = options.persist !== false
  const payload = internalColumns.value.map(column => ({ ...column }))
  if (persist) {
    persistState(payload)
  }
  emit("update", payload)
}

function handleReorder(columns: ColumnPanelState[]) {
  internalColumns.value = columns.map(column => ({ ...column }))
  emitUpdate()
}

function handleToggle(key: string, visible: boolean) {
  internalColumns.value = internalColumns.value.map(column =>
    column.key === key ? { ...column, visible } : column
  )
  emitUpdate()
}

function handleReset() {
  internalColumns.value = internalColumns.value.map(column => ({ ...column, visible: true }))
  emit("reset")
  const storage = createColumnStorageAccessor(props.storageKey)
  storage.clear()
  emitUpdate({ persist: false })
}

function emitClose() {
  emit("close")
}

function clampPosition(x: number, y: number) {
  const panel = panelRef.value
  const width = panel?.offsetWidth ?? 320
  const height = panel?.offsetHeight ?? 360
  const margin = 16
  const maxX = Math.max(margin, window.innerWidth - width - margin)
  const maxY = Math.max(margin, window.innerHeight - height - margin)
  position.x = Math.min(Math.max(x, margin), maxX)
  position.y = Math.min(Math.max(y, margin), maxY)
}

function alignTopRight() {
  const panel = panelRef.value
  if (!panel) return
  const margin = 24
  const width = panel.offsetWidth
  const x = Math.max(margin, window.innerWidth - width - margin)
  const y = margin
  clampPosition(x, y)
}

function startDrag(event: MouseEvent) {
  dragging.value = true
  hasCustomPosition.value = true
  dragOffset.x = event.clientX - position.x
  dragOffset.y = event.clientY - position.y
  window.addEventListener("mousemove", onDrag)
  window.addEventListener("mouseup", stopDrag)
}

function onDrag(event: MouseEvent) {
  if (!dragging.value) return
  clampPosition(event.clientX - dragOffset.x, event.clientY - dragOffset.y)
}

function stopDrag() {
  if (!dragging.value) return
  dragging.value = false
  window.removeEventListener("mousemove", onDrag)
  window.removeEventListener("mouseup", stopDrag)
}

function handleResize() {
  clampPosition(position.x, position.y)
}

function loadStoredState(): ColumnVisibilitySnapshot[] | null {
  const storage = createColumnStorageAccessor(props.storageKey)
  return storage.load()
}

function applyStoredState(stored: ColumnVisibilitySnapshot[]) {
  const orderMap = new Map<string, { index: number; visible: boolean }>()
  stored.forEach((entry, index) => {
    orderMap.set(entry.key, { index, visible: Boolean(entry.visible) })
  })

  const nextColumns = internalColumns.value
    .map(column => ({ ...column }))
    .sort((a, b) => {
      const orderA = orderMap.get(a.key)?.index ?? Number.MAX_SAFE_INTEGER
      const orderB = orderMap.get(b.key)?.index ?? Number.MAX_SAFE_INTEGER
      return orderA - orderB
    })
    .map(column => {
      const storedEntry = orderMap.get(column.key)
      return storedEntry ? { ...column, visible: storedEntry.visible } : column
    })

  internalColumns.value = nextColumns
}

let hydrated = false

watch(
  () => props.columns,
  newColumns => {
    internalColumns.value = mapColumns(newColumns)
    const stored = loadStoredState()
    if (stored) {
      applyStoredState(stored)
    }
    if (!hydrated) {
      emitUpdate()
      hydrated = true
    }
  },
  { immediate: true, deep: true }
)

onMounted(() => {
  requestAnimationFrame(() => {
    if (!hasCustomPosition.value) {
      alignTopRight()
    } else {
      clampPosition(position.x, position.y)
    }
  })
  window.addEventListener("resize", handleResize)
})

onBeforeUnmount(() => {
  stopDrag()
  window.removeEventListener("resize", handleResize)
})
</script>
