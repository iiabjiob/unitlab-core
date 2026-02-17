<template>
  <div
    class="inline-flex items-center rounded-md overflow-hidden bg-neutral-100 dark:bg-neutral-800 p-0.5"
    role="tablist"
    :aria-label="ariaLabel"
  >
    <button
      type="button"
      role="tab"
      :aria-selected="selectedMode === 'direct'"
      class="px-3 py-1 text-xs rounded-md transition-all select-none"
      :class="buttonClass('direct')"
      :disabled="disabled"
      @click="selectMode('direct')"
    >
      Direct
    </button>
    <button
      v-if="showSignal"
      type="button"
      role="tab"
      :aria-selected="selectedMode === 'signal'"
      class="px-3 py-1 text-xs rounded-md transition-all select-none"
      :class="buttonClass('signal')"
      :disabled="disabled"
      @click="selectMode('signal')"
    >
      By Signal
    </button>
  </div>
</template>

<script setup lang="ts">
import { computed, watch } from "vue"
import { useTabsController } from "@affino/tabs-vue"

type BindingMode = "direct" | "signal"

const props = withDefaults(defineProps<{
  modelValue: BindingMode
  showSignal?: boolean
  disabled?: boolean
  ariaLabel?: string
}>(), {
  showSignal: true,
  disabled: false,
  ariaLabel: "Binding mode",
})

const emit = defineEmits<{
  (e: "update:modelValue", value: BindingMode): void
}>()

const tabs = useTabsController<BindingMode>(props.modelValue === "signal" ? "signal" : "direct")

const selectedMode = computed<BindingMode>(() => (
  tabs.state.value.value === "signal" ? "signal" : "direct"
))

watch(
  () => props.modelValue,
  (nextMode) => {
    const normalized: BindingMode = nextMode === "signal" ? "signal" : "direct"
    if (tabs.state.value.value !== normalized) {
      tabs.select(normalized)
    }
  },
  { immediate: true },
)

watch(
  selectedMode,
  (nextMode) => {
    if (props.modelValue !== nextMode) {
      emit("update:modelValue", nextMode)
    }
  },
)

watch(
  () => props.showSignal,
  (allowed) => {
    if (!allowed && selectedMode.value === "signal") {
      tabs.select("direct")
    }
  },
  { immediate: true },
)

function selectMode(mode: BindingMode) {
  if (props.disabled) {
    return
  }
  if (mode === "signal" && !props.showSignal) {
    return
  }
  tabs.select(mode)
}

function buttonClass(mode: BindingMode) {
  const active = selectedMode.value === mode
  return active
    ? "bg-neutral-200 dark:bg-neutral-700 text-neutral-900 dark:text-neutral-50 ring-1 ring-neutral-300 dark:ring-neutral-600"
    : "text-neutral-600 dark:text-neutral-300 hover:bg-neutral-200/50 dark:hover:bg-neutral-700/50"
}
</script>