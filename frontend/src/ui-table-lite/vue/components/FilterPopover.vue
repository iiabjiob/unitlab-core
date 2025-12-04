<template>
  <div class="w-72 space-y-3 rounded-md border border-neutral-200 bg-white p-3 text-[11px] text-neutral-800 shadow-xl ring-1 ring-black/5 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100">
    <div class="space-y-1">
      <button
        type="button"
        class="flex w-full items-center justify-between rounded px-2 py-1.5 text-left text-[11px] font-medium transition hover:bg-blue-50 dark:hover:bg-neutral-800"
        :class="{ 'text-blue-600 dark:text-blue-400': sortDirection === 'asc' }"
        :disabled="!isSortable"
        @click="onSort('asc')"
      >
        <span>Sort A → Z</span>

      </button>
      <button
        type="button"
        class="flex w-full items-center justify-between rounded px-2 py-1.5 text-left text-[11px] font-medium transition hover:bg-blue-50 dark:hover:bg-neutral-800"
        :class="{ 'text-blue-600 dark:text-blue-400': sortDirection === 'desc' }"
        :disabled="!isSortable"
        @click="onSort('desc')"
      >
        <span>Sort Z → A</span>

      </button>

      <button
        type="button"
        class="flex w-full items-center justify-between rounded px-2 py-1.5 text-left text-[11px] font-medium transition hover:bg-blue-50 dark:hover:bg-neutral-800"
        :class="{ 'text-blue-600 dark:text-blue-400': groupActive }"
        :disabled="!isGroupable"
        @click="handleGroupToggle"
      >
        <span>{{ groupButtonLabel }}</span>
        <span aria-hidden="true" class="ml-1 text-[10px] leading-none">
          {{ groupActive ? '▼' : '›' }}
        </span>
      </button>

      <div class="relative space-y-1">
        <button
          type="button"
          class="flex w-full items-center justify-between rounded px-2 py-1.5 text-left text-[11px] font-medium transition hover:bg-blue-50 dark:hover:bg-neutral-800"
          @click="togglePinMenu"
        >
          <span>{{ pinLabel }}</span>
          <span aria-hidden="true" class="ml-1 text-[10px] leading-none">
            {{ pinMenuOpen ? '▼' : '›' }}
          </span>
        </button>
        <transition name="fade">
          <div
            v-if="pinMenuOpen"
            class="ml-2 space-y-1 border-l border-neutral-200 pl-2 dark:border-neutral-700"
          >
            <button
              type="button"
              class="flex w-full items-center justify-between rounded px-2 py-1 text-left text-[11px] transition hover:bg-blue-50 dark:hover:bg-neutral-800"
              :class="{ 'font-semibold text-blue-600 dark:text-blue-400': pinState === 'left' }"
              @click="handlePin('left')"
            >
              <span>Pin left</span>
            </button>
            <button
              type="button"
              class="flex w-full items-center justify-between rounded px-2 py-1 text-left text-[11px] transition hover:bg-blue-50 dark:hover:bg-neutral-800"
              :class="{ 'font-semibold text-blue-600 dark:text-blue-400': pinState === 'right' }"
              @click="handlePin('right')"
            >
              <span>Pin right</span>
            </button>
            <button
              type="button"
              class="flex w-full items-center justify-between rounded px-2 py-1 text-left text-[11px] transition hover:bg-blue-50 dark:hover:bg-neutral-800"
              :class="{ 'font-semibold text-blue-600 dark:text-blue-400': pinState === 'none' }"
              @click="handlePin('none')"
            >
              <span>Not pinned</span>
            </button>
          </div>
        </transition>
      </div>
    </div>

    <div class="h-px bg-neutral-200 dark:bg-neutral-700"></div>

    <div class="space-y-1">
      <button
        type="button"
        class="flex w-full items-center justify-between rounded px-2 py-1.5 text-left text-[11px] transition hover:bg-blue-50 dark:hover:bg-neutral-800"
        :class="{ 'font-semibold text-blue-600 dark:text-blue-400': hasAdvancedCondition }"
        @click="handleOpenAdvanced"
      >
        <span>Filter by condition</span>
  <span aria-hidden="true" class="ml-1 text-[10px] leading-none">›</span>
      </button>
      <p
        v-if="hasAdvancedCondition"
        class="px-2 text-[11px] text-blue-600 dark:text-blue-400"
      >
        Multiple conditions applied
      </p>
    </div>

    <div class="h-px bg-neutral-200 dark:bg-neutral-700"></div>

    <div class="relative mt-1">
      <input
        :ref="searchInputRefHandler"
        v-model="searchProxy"
        type="text"
        placeholder="Search"
        :name="fieldName('search')"
        autocomplete="off"
        autocapitalize="none"
        autocorrect="off"
        spellcheck="false"
        class="h-8 w-full rounded-sm border border-neutral-200 bg-white pl-2 pr-6 text-[11px] text-neutral-700 outline-none transition focus:border-blue-500 focus:ring-0 dark:border-neutral-600 dark:bg-neutral-800 dark:text-neutral-100"
      />
  <span aria-hidden="true" class="pointer-events-none absolute right-2 top-1.5 text-[11px] text-neutral-400 dark:text-neutral-500">🔍</span>
    </div>

    <div class="relative rounded-sm">
      <VirtualList
        ref="optionsListRef"
        :items="virtualItems"
        :item-height="ROW_HEIGHT"
        :height="virtualListHeight"
        :overscan="virtualOverscan"
        class="rounded-sm"
      >
        <template #default="slot">
          <label
            :class="rowClass(slot.index, toVirtualRow(slot.item))"
            :style="rowStyle"
          >
            <template v-if="isSelectAllRow(toVirtualRow(slot.item))">
              <div class="flex items-center gap-2">
                <input
                  ref="selectAllCheckbox"
                  type="checkbox"
                  class="h-3.5 w-3.5 rounded border-neutral-400 text-blue-600 focus:outline-none focus:ring-blue-500 dark:border-neutral-500"
                  :checked="isSelectAllChecked"
                  :name="fieldName('select-all')"
                  @change="handleSelectAllChange"
                />
                <span>Select all</span>
              </div>
            </template>
            <template v-else>
              <div class="flex items-center gap-2">
                <input
                  type="checkbox"
                  class="h-3.5 w-3.5 rounded border-neutral-400 text-blue-600 focus:outline-none focus:ring-blue-500 dark:border-neutral-500"
                  :checked="selectedSet.has(toOptionRow(slot.item).option.key)"
                  :name="fieldName(`option-${toOptionRow(slot.item).option.key}`)"
                  @change="() => emit('toggle-option', toOptionRow(slot.item).option.key)"
                />
                <span class="truncate" :title="toOptionRow(slot.item).option.label">
                  {{ toOptionRow(slot.item).option.label }}
                </span>
              </div>
            </template>
          </label>
        </template>
      </VirtualList>

      <transition name="fade">
        <div
          v-if="loadingState"
          class="absolute inset-0 z-10 flex items-center justify-center bg-white/75 dark:bg-neutral-900/75"
        >
          <LoadingSpinner size="sm" />
        </div>
      </transition>

      <p
        v-if="!loadingState && !filteredOptions.length"
        class="py-4 text-center text-[11px] text-neutral-400 dark:text-neutral-500"
      >
        No matching values
      </p>
    </div>

    <button
      type="button"
      class="w-full rounded px-2 py-1.5 text-left text-[11px] font-medium text-blue-600 transition hover:bg-blue-50 dark:text-blue-400 dark:hover:bg-neutral-800"
      @click="handleClearFilter"
    >
      Clear filter from {{ displayColumnLabel }}
    </button>

    <div class="flex items-center justify-end gap-2 pt-2 text-[11px]">
      <button
        type="button"
        class="ui-table__button ui-table__button--neutral"
        @click="onCancel"
      >
        Cancel
      </button>
      <button
        type="button"
        class="ui-table__button ui-table__button--primary"
        @click="onApply"
      >
        Apply
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue"
import type { ComponentPublicInstance, VNodeRef } from "vue"
import VirtualList from "@/components/ui/VirtualList.vue"
import type { VirtualListExposedMethods } from "@/components/ui/VirtualList.types"
import type { FilterCondition, FilterOption } from "../composables/useTableFilters"
import LoadingSpinner from "@/components/ui/LoadingSpinner.vue"

type LoadOptionsArgs = { search: string }
type LoadOptionsFn = (args: LoadOptionsArgs) => Promise<FilterOption[]> | FilterOption[]

type ColumnPinPosition = "left" | "right" | "none"

const props = defineProps<{
  col: any
  close: () => void
  options?: FilterOption[]
  loadOptions?: LoadOptionsFn
  selectedKeys: string[]
  search: string
  isSelectAllChecked: boolean
  isSelectAllIndeterminate: boolean
  sortDirection: string | null
  registerSearchInput?: (element: Element | { $el?: Element | null } | null) => void
  loading?: boolean
  filterCondition?: FilterCondition | null
  groupActive?: boolean
  pinState?: ColumnPinPosition
  searchDebounce?: number
}>()

const emit = defineEmits([
  "update:search",
  "toggle-option",
  "toggle-select-all",
  "apply",
  "cancel",
  "sort",
  "reset",
  "open-advanced",
  "group",
  "pin",
])

const searchProxy = computed({
  get: () => props.search,
  set: value => emit("update:search", value),
})

const selectedSet = computed(() => new Set(props.selectedKeys))
const displayColumnLabel = computed(() => props.col?.label ?? "this column")
const isGroupable = computed(() => Boolean(props.col) && props.col.groupable !== false && !props.col.isSystem)
const groupActive = computed(() => Boolean(props.groupActive))
const groupButtonLabel = computed(() => (groupActive.value ? `Ungroup ${displayColumnLabel.value}` : `Group by ${displayColumnLabel.value}`))
const pinLabel = computed(() => {
  switch (pinState.value) {
    case "left":
      return "Pinned left"
    case "right":
      return "Pinned right"
    default:
      return "Not pinned"
  }
})
const columnKeySlug = computed(() => {
  const raw = props.col?.key ?? props.col?.id ?? props.col?.field ?? props.col?.label ?? "column"
  const slug = String(raw).toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/(^-|-$)/g, "")
  return slug || "column"
})

const hasAdvancedCondition = computed(() => Boolean(props.filterCondition?.clauses?.length))
const isSortable = computed(() => props.col?.sortable !== false)

const optionsListRef = ref<VirtualListExposedMethods | null>(null)
const selectAllCheckbox = ref<HTMLInputElement | null>(null)
const optionsState = ref<FilterOption[]>([])
const internalLoading = ref(Boolean(props.loadOptions))
const activeRequestId = ref(0)
const pendingSearchTimeout = ref<number | null>(null)
const debouncedSearch = ref(props.search)
const pinMenuOpen = ref(false)
const pinState = computed<ColumnPinPosition>(() => props.pinState ?? "none")

const ROW_HEIGHT = 28
const MAX_LIST_HEIGHT = 14 * 16 // Tailwind h-56 equivalent
const MIN_VIRTUALIZE_COUNT = 12

type SelectAllRow = { key: string; kind: "selectAll" }
type OptionRow = { key: string; kind: "option"; option: FilterOption }
type VirtualRow = SelectAllRow | OptionRow

const filteredOptions = computed<FilterOption[]>(() => {
  const source = optionsState.value
  const query = props.search.trim().toLowerCase()
  if (!query) return source
  return source.filter(option => {
    const label = typeof option.label === "string" ? option.label.toLowerCase() : String(option.label ?? "").toLowerCase()
    if (label.includes(query)) return true
    const key = typeof option.key === "string" ? option.key.toLowerCase() : String(option.key ?? "").toLowerCase()
    return key.includes(query)
  })
})

const virtualItems = computed<VirtualRow[]>(() => {
  const rows: VirtualRow[] = [{ key: "__select_all__", kind: "selectAll" }]
  for (const option of filteredOptions.value) {
    rows.push({ key: option.key, kind: "option", option })
  }
  return rows
})

const virtualOverscan = computed(() => (filteredOptions.value.length < MIN_VIRTUALIZE_COUNT ? 0 : undefined))
const virtualListHeight = computed(() => {
  const totalRows = virtualItems.value.length
  const naturalHeight = totalRows * ROW_HEIGHT
  return Math.max(Math.min(naturalHeight, MAX_LIST_HEIGHT), ROW_HEIGHT)
})

const rowBaseClass =
  "flex w-full items-center justify-between gap-2 px-2 py-1.5 text-[11px] text-neutral-700 transition hover:bg-blue-50 dark:text-neutral-100 dark:hover:bg-neutral-800"
const rowStyle = { height: `${ROW_HEIGHT}px` } as const
const lastRowIndex = computed(() => virtualItems.value.length - 1)

function isSelectAllRow(item: VirtualRow): item is SelectAllRow {
  return item.kind === "selectAll"
}

function rowClass(index: number, item: VirtualRow) {
  const classes = [rowBaseClass]
  if (isSelectAllRow(item)) {
    classes.push("font-medium")
  }
  if (index < lastRowIndex.value) {
    classes.push("border-b border-neutral-100 dark:border-neutral-800")
  }
  return classes.join(" ")
}

function toVirtualRow(value: unknown): VirtualRow {
  return value as VirtualRow
}

function toOptionRow(value: unknown): OptionRow {
  return value as OptionRow
}

function scrollToFirstFound() {
  if (!optionsListRef.value) return
  if (!filteredOptions.value.length) {
    optionsListRef.value.scrollToTop()
    return
  }
  const hasSearch = debouncedSearch.value.trim().length > 0 || props.search.trim().length > 0
  if (!hasSearch) {
    optionsListRef.value.scrollToTop()
    return
  }
  optionsListRef.value.scrollTo(1)
}

const loadingState = computed(() => (props.loadOptions ? internalLoading.value : Boolean(props.loading)))

function fieldName(suffix: string) {
  return `filter-${columnKeySlug.value}-${suffix}`
}

function resolveElement(element: Element | ComponentPublicInstance | null): Element | null {
  if (!element) return null
  if (element instanceof Element) return element
  const maybeEl = element.$el
  return maybeEl instanceof Element ? maybeEl : null
}

function syncSelectAllCheckboxState() {
  const checkbox = selectAllCheckbox.value
  if (!checkbox) return
  checkbox.indeterminate = Boolean(props.isSelectAllIndeterminate)
  checkbox.checked = Boolean(props.isSelectAllChecked)
}

function clearPendingSearchTimeout() {
  if (pendingSearchTimeout.value !== null) {
    window.clearTimeout(pendingSearchTimeout.value)
    pendingSearchTimeout.value = null
  }
}

function queueSearch(value: string, immediate = false) {
  if (!props.loadOptions) {
    debouncedSearch.value = value
    return
  }
  const delay = typeof props.searchDebounce === "number" ? Math.max(0, props.searchDebounce) : 200
  if (!immediate && value === debouncedSearch.value) {
    internalLoading.value = false
    clearPendingSearchTimeout()
    return
  }
  internalLoading.value = true
  if (immediate || delay === 0) {
    clearPendingSearchTimeout()
    const sameValue = debouncedSearch.value === value
    debouncedSearch.value = value
    if (sameValue) {
      void runLoad(value)
    }
    return
  }
  clearPendingSearchTimeout()
  pendingSearchTimeout.value = window.setTimeout(() => {
    pendingSearchTimeout.value = null
    debouncedSearch.value = value
  }, delay)
}

async function runLoad(search: string) {
  const loader = props.loadOptions
  if (!loader) {
    optionsState.value = Array.isArray(props.options) ? [...props.options] : []
    await nextTick()
    syncSelectAllCheckboxState()
    optionsListRef.value?.scrollToTop()
    internalLoading.value = false
    return
  }
  const requestId = activeRequestId.value + 1
  activeRequestId.value = requestId
  try {
    const result = await loader({ search })
    if (activeRequestId.value !== requestId) return
    optionsState.value = Array.isArray(result) ? [...result] : []
    await nextTick()
    syncSelectAllCheckboxState()
    optionsListRef.value?.scrollToTop()
  } catch {
    if (activeRequestId.value !== requestId) return
    optionsState.value = []
  } finally {
    if (activeRequestId.value === requestId) {
      internalLoading.value = false
    }
  }
}

function applyOptionsFromProps() {
  if (props.loadOptions) return
  optionsState.value = Array.isArray(props.options) ? [...props.options] : []
  nextTick(() => {
    syncSelectAllCheckboxState()
    optionsListRef.value?.scrollToTop()
  })
  internalLoading.value = false
}

watch(
  () => props.isSelectAllIndeterminate,
  () => {
    syncSelectAllCheckboxState()
  },
  { immediate: true }
)

watch(
  () => props.isSelectAllChecked,
  () => {
    syncSelectAllCheckboxState()
  },
  { immediate: true }
)

watch(selectAllCheckbox, () => {
  syncSelectAllCheckboxState()
})

watch(
  () => props.options,
  () => {
    if (!props.loadOptions) {
      applyOptionsFromProps()
    }
  },
  { immediate: true }
)

watch(optionsState, async () => {
  await nextTick()
  optionsListRef.value?.scrollToTop()
})

watch(filteredOptions, async () => {
  await nextTick()
  scrollToFirstFound()
})

watch(
  () => props.search,
  value => {
    queueSearch(value)
  }
)

watch(
  () => props.col?.key,
  () => {
    optionsState.value = []
    if (props.loadOptions) {
      queueSearch(props.search, true)
    } else {
      applyOptionsFromProps()
    }
  }
)

watch(
  () => props.loadOptions,
  () => {
    optionsState.value = []
    internalLoading.value = Boolean(props.loadOptions)
    queueSearch(props.search, true)
  }
)

watch(debouncedSearch, value => {
  runLoad(value)
})

const searchInputRefHandler: VNodeRef = element => {
  const resolved = resolveElement(element as Element | ComponentPublicInstance | null)
  props.registerSearchInput?.(resolved)
}

onMounted(() => {
  queueSearch(props.search, true)
})

onBeforeUnmount(() => {
  clearPendingSearchTimeout()
  props.registerSearchInput?.(null)
})

function onApply() {
  emit("apply")
  props.close()
}

function onCancel() {
  emit("cancel")
  props.close()
}

function onSort(direction: "asc" | "desc") {
  if (!isSortable.value) return
  emit("sort", direction)
  props.close()
}

function handleGroupToggle() {
  if (!isGroupable.value) return
  emit("group")
  props.close()
}

function handleSelectAllChange(event: Event) {
  const input = event.target as HTMLInputElement | null
  emit("toggle-select-all", Boolean(input?.checked))
}

function handleClearFilter() {
  emit("toggle-select-all", false)
  emit("reset")
  props.close()
}

function handleOpenAdvanced() {
  emit("open-advanced")
  props.close()
}

function togglePinMenu() {
  pinMenuOpen.value = !pinMenuOpen.value
}

function handlePin(position: ColumnPinPosition) {
  emit("pin", position)
  pinMenuOpen.value = false
  props.close()
}
</script>
