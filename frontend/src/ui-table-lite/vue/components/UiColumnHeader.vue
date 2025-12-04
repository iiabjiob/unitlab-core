<template>
  <div v-bind="attrs">
    <div
      v-if="showLeftPlaceholder"
      class="ui-table-header-filler ui-table-header-filler--left"
      :style="{ width: leftPaddingStyle }"
      aria-hidden="true"
    ></div>
    <div
      :class="headerClassList as any"
      :style="headerStyle"
      :data-column-key="columnKey"
      :title="title"
      role="columnheader"
      :aria-sort="ariaSort"
      :aria-colindex="ariaColIndex"
      :aria-rowindex="ariaRowIndex"
      :tabindex="tabIndex"
      @click="event => emit('header-click', event)"
      @keydown="event => emit('header-keydown', event as KeyboardEvent)"
      :ref="setHeaderCellRef"
    >
      <div class="relative flex flex-col items-stretch gap-0.5 w-full">
        <div class="flex items-center gap-1 w-full">
          <div :class="['flex items-center gap-1 flex-1 min-w-0', headerJustifyClass]">
            <span
              v-if="grouped"
              class="flex items-center gap-0.5 flex-shrink-0 text-amber-600 dark:text-amber-300"
            >
              <svg
                class="h-3 w-3"
                viewBox="0 0 16 16"
                fill="none"
                stroke="currentColor"
                stroke-width="1.5"
                stroke-linecap="round"
                stroke-linejoin="round"
              >
                <path d="M3 4h7M3 8h10M3 12h5" />
              </svg>
              <span
                v-if="groupOrder != null"
                class="text-[9px] font-semibold leading-none"
              >
                {{ groupOrder }}
              </span>
            </span>
            <span
              class="text-xs font-semibold leading-none truncate flex-1 min-w-0"
              :class="headerTextClass"
              data-auto-resize-target
            >
              {{ displayText }}
            </span>
            <svg
              v-if="sortDirection === 'asc'"
              class="h-3 w-3 text-blue-500 dark:text-blue-300"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
            >
              <path stroke-linecap="round" stroke-linejoin="round" d="M7 15l5-5 5 5" />
            </svg>
            <svg
              v-else-if="sortDirection === 'desc'"
              class="h-3 w-3 text-blue-500 dark:text-blue-300"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
            >
              <path stroke-linecap="round" stroke-linejoin="round" d="M7 9l5 5 5-5" />
            </svg>
          </div>
          <button
            v-if="!isMenuDisabled"
            :ref="setMenuButtonRef"
            type="button"
            class="ui-table__icon-button ui-table__icon-button--menu ml-auto flex-shrink-0"
            draggable="false"
            :class="[
              filterButtonClass,
              isMenuOpen ? 'ui-table__icon-button--active' : ''
            ]"
            aria-label="Open column menu"
            aria-haspopup="menu"
            :aria-expanded="isMenuOpen ? 'true' : 'false'"
            @click.stop="emit('menu-toggle')"
            @keydown.enter.prevent.stop="emit('menu-toggle')"
            @keydown.space.prevent.stop="emit('menu-toggle')"
          >
            <span
              v-if="sortPriorityLabel"
              class="absolute -top-1 -right-1 flex h-3.5 min-w-[14px] items-center justify-center rounded-full bg-blue-500 text-[9px] font-semibold leading-none text-white shadow-sm dark:bg-blue-400"
            >
              {{ sortPriorityLabel }}
            </span>
            <svg
              v-if="filterActive"
              class="h-3 w-3"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
            >
              <path stroke-linecap="round" stroke-linejoin="round" d="M4 4h16l-6 7v6l-4-2v-4L4 4z" />
            </svg>
            <svg
              v-else
              class="h-3 w-3"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
            >
              <path stroke-linecap="round" stroke-linejoin="round" d="M6 9l6 6 6-6" />
            </svg>
          </button>
        </div>
        <span
          v-if="showSecondary"
          class="text-[10px] text-gray-500 dark:text-gray-400 font-normal leading-tight truncate w-full"
          :class="headerTextClass"
        >
          {{ secondaryLabel }}
        </span>

        <teleport v-if="!isMenuDisabled" to="body">
          <Transition
            enter-active-class="transition ease-out duration-100"
            enter-from-class="opacity-0 -translate-y-1"
            enter-to-class="opacity-100 translate-y-0"
            leave-active-class="transition ease-in duration-75"
            leave-from-class="opacity-100 translate-y-0"
            leave-to-class="opacity-0 -translate-y-1"
          >
            <div
              v-if="isMenuOpen"
              :ref="setPopoverPanelRef"
              :style="popoverPanelStyle"
              class="fixed z-50 mt-1 w-64 rounded-md border border-neutral-300 bg-white text-left shadow-xl dark:border-neutral-700 dark:bg-neutral-900"
              @click.stop
            >
              <slot name="menu" v-bind="menuSlotBindings" />
            </div>
          </Transition>
        </teleport>
      </div>

      <div
        v-if="showResizeHandle"
        class="ui-table-column-resize absolute top-0 h-full z-10"
        :style="resizeHandleStyle"
        @mousedown.prevent="event => emit('resize-start', event as MouseEvent)"
        @dblclick.stop="event => emit('resize-auto', event as MouseEvent)"
        @click.stop
      />
    </div>
    <div
      v-if="showRightPlaceholder"
      class="ui-table-header-filler ui-table-header-filler--right"
      :style="{ width: rightPaddingStyle }"
      aria-hidden="true"
    ></div>
  </div>
</template>

<script setup lang="ts">
import { useAttrs, type ComponentPublicInstance, type StyleValue } from "vue"
import type { UiTableColumn } from "../../core/types"

export interface ColumnHeaderMenuBindings {
  close: () => void
  col: UiTableColumn
  sortDirection: "asc" | "desc" | null
}

const attrs = useAttrs()

const props = withDefaults(
  defineProps<{
  headerClassList: unknown
  headerStyle: StyleValue
    columnKey: string
  title?: string
    displayText: string
    secondaryLabel?: string | null
    headerTextClass: string
    headerJustifyClass: string
    grouped?: boolean
    groupOrder?: number | null
    sortDirection: "asc" | "desc" | null
    sortPriorityLabel?: string | null
    isMenuDisabled?: boolean
    filterActive?: boolean
    filterButtonClass?: string
    isMenuOpen?: boolean
    showSecondary?: boolean
    menuSlotBindings?: ColumnHeaderMenuBindings
    popoverPanelStyle?: Record<string, string>
    showLeftPlaceholder?: boolean
    leftPaddingStyle?: string
    showRightPlaceholder?: boolean
    rightPaddingStyle?: string
    tabIndex?: number
    ariaSort?: "ascending" | "descending" | "none"
    ariaColIndex?: number | string
    ariaRowIndex?: number | string
  menuButtonRefSetter?: (element: HTMLElement | null) => void
  popoverPanelRefSetter?: (element: HTMLElement | null) => void
  headerCellRefSetter?: (element: HTMLElement | null) => void
    showResizeHandle?: boolean
    resizeHandleStyle?: Record<string, string>
  }>(),
  {
  title: undefined,
    secondaryLabel: null,
    grouped: false,
    groupOrder: null,
    sortPriorityLabel: null,
    isMenuDisabled: false,
    filterActive: false,
    filterButtonClass: "",
    isMenuOpen: false,
    showSecondary: false,
    menuSlotBindings: undefined,
    popoverPanelStyle: undefined,
    showLeftPlaceholder: false,
    leftPaddingStyle: "0px",
    showRightPlaceholder: false,
    rightPaddingStyle: "0px",
    tabIndex: -1,
    ariaSort: "none",
    ariaColIndex: undefined,
    ariaRowIndex: 1,
  menuButtonRefSetter: undefined,
  popoverPanelRefSetter: undefined,
  headerCellRefSetter: undefined,
    showResizeHandle: false,
    resizeHandleStyle: undefined,
  },
)

const emit = defineEmits<{
  (e: "header-click", event: MouseEvent | KeyboardEvent): void
  (e: "header-keydown", event: KeyboardEvent): void
  (e: "menu-toggle"): void
  (e: "resize-start", event: MouseEvent): void
  (e: "resize-auto", event: MouseEvent): void
}>()

function toHTMLElement(element: Element | ComponentPublicInstance | null): HTMLElement | null {
  if (element instanceof HTMLElement) {
    return element
  }
  if (element && typeof element === "object" && "$el" in element) {
    const maybe = (element as ComponentPublicInstance).$el
    return maybe instanceof HTMLElement ? maybe : null
  }
  return null
}

function setHeaderCellRef(element: Element | ComponentPublicInstance | null, _refs?: Record<string, unknown>) {
  if (!props.headerCellRefSetter) return
  props.headerCellRefSetter(toHTMLElement(element))
}

function setMenuButtonRef(element: Element | ComponentPublicInstance | null, _refs?: Record<string, unknown>) {
  if (!props.menuButtonRefSetter) return
  props.menuButtonRefSetter(toHTMLElement(element))
}

function setPopoverPanelRef(element: Element | ComponentPublicInstance | null, _refs?: Record<string, unknown>) {
  if (!props.popoverPanelRefSetter) return
  props.popoverPanelRefSetter(toHTMLElement(element))
}
</script>
