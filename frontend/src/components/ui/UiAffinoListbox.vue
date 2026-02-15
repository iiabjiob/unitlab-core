<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, ref, watch } from "vue"
import type { ComponentPublicInstance } from "vue"

type ListboxValue = string | number | null

type ListboxOption = {
  value: ListboxValue
  label: string
  disabled?: boolean
}

const props = withDefaults(defineProps<{
  modelValue?: ListboxValue
  options: ListboxOption[]
  placeholder?: string
  disabled?: boolean
  ariaLabel?: string
  name?: string
}>(), {
  modelValue: null,
  placeholder: "Select",
  disabled: false,
  ariaLabel: "Listbox",
  name: undefined,
})

const emit = defineEmits<{
  (event: "update:modelValue", value: ListboxValue): void
  (event: "change", value: ListboxValue): void
  (event: "blur"): void
}>()

const rootRef = ref<HTMLElement | null>(null)
const triggerRef = ref<HTMLButtonElement | null>(null)
const panelRef = ref<HTMLElement | null>(null)
const isOpen = ref(false)
const activeIndex = ref(-1)
const optionRefs = new Map<number, HTMLElement>()
const typeahead = ref("")
const panelStyle = ref<Record<string, string>>({})
let typeaheadTimer: ReturnType<typeof setTimeout> | null = null

const componentId = `ui-affino-listbox-${Math.random().toString(36).slice(2, 10)}`
const triggerId = `${componentId}-trigger`
const listboxId = `${componentId}-listbox`

const normalizedOptions = computed(() => props.options ?? [])

const selectedIndex = computed(() =>
  normalizedOptions.value.findIndex(option => valuesEqual(option.value, props.modelValue)),
)

const selectedOption = computed(() => {
  const index = selectedIndex.value
  return index >= 0 ? (normalizedOptions.value[index] ?? null) : null
})

const hiddenInputValue = computed(() => {
  if (props.modelValue === null || typeof props.modelValue === "undefined") return ""
  return String(props.modelValue)
})

watch(
  [selectedIndex, normalizedOptions],
  ([nextSelected]) => {
    if (!normalizedOptions.value.length) {
      activeIndex.value = -1
      return
    }
    if (nextSelected >= 0 && !normalizedOptions.value[nextSelected]?.disabled) {
      activeIndex.value = nextSelected
      return
    }
    activeIndex.value = firstEnabledIndex()
  },
  { immediate: true },
)

watch(
  () => [isOpen.value, activeIndex.value] as const,
  ([open, index]) => {
    if (!open || index < 0) return
    nextTick(() => {
      optionRefs.get(index)?.scrollIntoView({ block: "nearest" })
    })
  },
)

watch(isOpen, (open) => {
  if (typeof window === "undefined") return
  if (open) {
    window.addEventListener("pointerdown", onDocumentPointerDown, true)
    window.addEventListener("resize", updatePanelPosition, { passive: true })
    window.addEventListener("scroll", updatePanelPosition, true)
    nextTick(() => {
      updatePanelPosition()
    })
    return
  }
  window.removeEventListener("pointerdown", onDocumentPointerDown, true)
  window.removeEventListener("resize", updatePanelPosition)
  window.removeEventListener("scroll", updatePanelPosition, true)
})

onBeforeUnmount(() => {
  if (typeof window !== "undefined") {
    window.removeEventListener("pointerdown", onDocumentPointerDown, true)
    window.removeEventListener("resize", updatePanelPosition)
    window.removeEventListener("scroll", updatePanelPosition, true)
  }
  if (typeaheadTimer) {
    clearTimeout(typeaheadTimer)
    typeaheadTimer = null
  }
})

function valuesEqual(left: ListboxValue | undefined, right: ListboxValue | undefined): boolean {
  if (left === null || typeof left === "undefined" || right === null || typeof right === "undefined") {
    return left === right
  }
  return String(left) === String(right)
}

function firstEnabledIndex(): number {
  return normalizedOptions.value.findIndex(option => !option.disabled)
}

function lastEnabledIndex(): number {
  for (let index = normalizedOptions.value.length - 1; index >= 0; index -= 1) {
    if (!normalizedOptions.value[index]?.disabled) {
      return index
    }
  }
  return -1
}

function optionId(index: number): string {
  return `${componentId}-option-${index}`
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

function openList(preferredIndex?: number) {
  if (props.disabled || !normalizedOptions.value.length) return
  isOpen.value = true
  if (typeof preferredIndex === "number") {
    activeIndex.value = clampToEnabled(preferredIndex)
    return
  }
  if (selectedIndex.value >= 0 && !normalizedOptions.value[selectedIndex.value]?.disabled) {
    activeIndex.value = selectedIndex.value
    nextTick(() => {
      updatePanelPosition()
    })
    return
  }
  activeIndex.value = firstEnabledIndex()
  nextTick(() => {
    updatePanelPosition()
  })
}

function closeList(emitBlur = false) {
  isOpen.value = false
  resetTypeahead()
  if (emitBlur) {
    emit("blur")
  }
}

function toggleList() {
  if (props.disabled) return
  if (isOpen.value) {
    closeList()
    return
  }
  openList()
}

function resetTypeahead() {
  typeahead.value = ""
  if (typeaheadTimer) {
    clearTimeout(typeaheadTimer)
    typeaheadTimer = null
  }
}

function queueTypeaheadReset() {
  if (typeaheadTimer) {
    clearTimeout(typeaheadTimer)
  }
  typeaheadTimer = setTimeout(() => {
    typeahead.value = ""
    typeaheadTimer = null
  }, 400)
}

function clampToEnabled(index: number): number {
  if (!normalizedOptions.value.length) return -1
  const bounded = Math.max(0, Math.min(normalizedOptions.value.length - 1, index))
  if (!normalizedOptions.value[bounded]?.disabled) {
    return bounded
  }
  return findNextEnabled(bounded, 1)
}

function findNextEnabled(from: number, delta: 1 | -1): number {
  const total = normalizedOptions.value.length
  if (!total) return -1

  for (let step = 1; step <= total; step += 1) {
    const candidate = (from + step * delta + total) % total
    if (!normalizedOptions.value[candidate]?.disabled) {
      return candidate
    }
  }
  return -1
}

function moveFocus(delta: 1 | -1) {
  if (!normalizedOptions.value.length) return
  if (activeIndex.value < 0) {
    activeIndex.value = delta > 0 ? firstEnabledIndex() : lastEnabledIndex()
    return
  }
  activeIndex.value = findNextEnabled(activeIndex.value, delta)
}

function commitIndex(index: number) {
  const option = normalizedOptions.value[index]
  if (!option || option.disabled) return
  emit("update:modelValue", option.value)
  emit("change", option.value)
  closeList()
}

function onTriggerKeydown(event: KeyboardEvent) {
  if (props.disabled) return

  switch (event.key) {
    case "ArrowDown":
      event.preventDefault()
      if (!isOpen.value) {
        openList()
      }
      moveFocus(1)
      return
    case "ArrowUp":
      event.preventDefault()
      if (!isOpen.value) {
        openList()
      }
      moveFocus(-1)
      return
    case "Home":
      event.preventDefault()
      openList(firstEnabledIndex())
      return
    case "End":
      event.preventDefault()
      openList(lastEnabledIndex())
      return
    case "Enter":
    case " ":
      event.preventDefault()
      if (!isOpen.value) {
        openList()
        return
      }
      if (activeIndex.value >= 0) {
        commitIndex(activeIndex.value)
      }
      return
    case "Escape":
      if (!isOpen.value) return
      event.preventDefault()
      closeList()
      return
    case "Tab":
      closeList(true)
      return
    default:
      break
  }

  if (!isOpen.value || event.key.length !== 1 || event.altKey || event.ctrlKey || event.metaKey) {
    return
  }

  typeahead.value += event.key.toLowerCase()
  queueTypeaheadReset()

  const start = activeIndex.value >= 0 ? activeIndex.value : -1
  const total = normalizedOptions.value.length
  for (let offset = 1; offset <= total; offset += 1) {
    const candidate = (start + offset + total) % total
    const option = normalizedOptions.value[candidate]
    if (!option || option.disabled) continue
    if (option.label.toLowerCase().startsWith(typeahead.value)) {
      activeIndex.value = candidate
      break
    }
  }
}

function onOptionPointerDown(index: number, event: PointerEvent) {
  event.preventDefault()
  activeIndex.value = index
  commitIndex(index)
}

function onOptionMouseMove(index: number) {
  if (normalizedOptions.value[index]?.disabled) return
  activeIndex.value = index
}

function onFocusOut(event: FocusEvent) {
  const nextFocused = event.relatedTarget as Node | null
  if (rootRef.value && nextFocused && rootRef.value.contains(nextFocused)) {
    return
  }
  if (panelRef.value && nextFocused && panelRef.value.contains(nextFocused)) {
    return
  }
  closeList(true)
}

function onDocumentPointerDown(event: PointerEvent) {
  if (!isOpen.value) return
  const target = event.target as Node | null
  if (rootRef.value && target && rootRef.value.contains(target)) {
    return
  }
  if (panelRef.value && target && panelRef.value.contains(target)) {
    return
  }
  closeList()
}

function updatePanelPosition() {
  if (!isOpen.value || typeof window === "undefined") return
  const trigger = triggerRef.value
  if (!trigger) return

  const rect = trigger.getBoundingClientRect()
  const gap = 6
  const viewportHeight = window.innerHeight
  const viewportWidth = window.innerWidth
  const desiredMaxHeight = 240
  const spaceBelow = viewportHeight - rect.bottom - gap
  const spaceAbove = rect.top - gap
  const openUpward = spaceBelow < 160 && spaceAbove > spaceBelow
  const maxHeight = Math.max(120, Math.min(desiredMaxHeight, openUpward ? spaceAbove : spaceBelow))

  const left = Math.min(Math.max(8, rect.left), Math.max(8, viewportWidth - rect.width - 8))
  const top = openUpward ? Math.max(8, rect.top - gap - maxHeight) : Math.min(viewportHeight - 8 - maxHeight, rect.bottom + gap)

  panelStyle.value = {
    position: "fixed",
    left: `${left}px`,
    top: `${top}px`,
    width: `${rect.width}px`,
    maxHeight: `${maxHeight}px`,
    zIndex: "1105",
  }
}
</script>

<template>
  <div
    ref="rootRef"
    class="relative w-full"
    @focusout="onFocusOut"
  >
    <button
      ref="triggerRef"
      :id="triggerId"
      type="button"
      class="w-full rounded-lg border border-neutral-300 bg-white px-3 py-2 text-left text-sm text-neutral-900 transition focus:outline-none focus:ring-2 focus:ring-primary-500/30 disabled:cursor-not-allowed disabled:opacity-60 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100"
      :aria-label="ariaLabel"
      aria-haspopup="listbox"
      :aria-expanded="isOpen ? 'true' : 'false'"
      :aria-controls="isOpen ? listboxId : undefined"
      :disabled="disabled"
      @click="toggleList"
      @keydown="onTriggerKeydown"
    >
      <span :class="selectedOption ? 'text-neutral-900 dark:text-neutral-100' : 'text-neutral-500 dark:text-neutral-400'">
        {{ selectedOption?.label ?? placeholder }}
      </span>
      <span class="pointer-events-none absolute right-3 top-1/2 -translate-y-1/2 text-neutral-500 dark:text-neutral-400" aria-hidden="true">
        ▾
      </span>
    </button>

    <input v-if="name" :name="name" type="hidden" :value="hiddenInputValue">

    <teleport to="body">
      <div
        v-if="isOpen"
        ref="panelRef"
        :id="listboxId"
        role="listbox"
        :aria-labelledby="triggerId"
        tabindex="-1"
        class="overflow-auto rounded-lg border border-neutral-200 bg-white shadow-lg dark:border-neutral-700 dark:bg-neutral-900"
        :style="panelStyle"
      >
        <button
          v-for="(option, index) in normalizedOptions"
          :id="optionId(index)"
          :key="`${typeof option.value}:${String(option.value)}`"
          :ref="(el) => setOptionRef(index, el)"
          type="button"
          role="option"
          :aria-selected="selectedIndex === index ? 'true' : 'false'"
          :disabled="option.disabled"
          class="flex w-full items-center justify-between px-3 py-2 text-left text-sm text-neutral-800 transition-colors hover:bg-neutral-50 disabled:cursor-not-allowed disabled:opacity-45 dark:text-neutral-100 dark:hover:bg-neutral-800/60"
          :class="{
            'bg-primary-50 text-primary-700 dark:bg-primary-500/20 dark:text-primary-200': selectedIndex === index,
            'bg-neutral-50 dark:bg-neutral-800/60': activeIndex === index && selectedIndex !== index,
          }"
          @pointerdown="onOptionPointerDown(index, $event)"
          @mousemove="onOptionMouseMove(index)"
        >
          {{ option.label }}
        </button>
        <div v-if="!normalizedOptions.length" class="px-3 py-2 text-sm text-neutral-500 dark:text-neutral-400">
          No options
        </div>
      </div>
    </teleport>
  </div>
</template>