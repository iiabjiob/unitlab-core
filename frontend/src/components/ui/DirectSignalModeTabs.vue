<template>
  <div
    class="direct-signal-mode-tabs"
    role="tablist"
    :aria-label="ariaLabel"
  >
    <button
      type="button"
      role="tab"
      :aria-selected="selectedMode === 'direct'"
      class="direct-signal-mode-tabs__button"
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
      class="direct-signal-mode-tabs__button"
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
  return active ? "is-active" : "is-idle"
}
</script>

<style scoped>
.direct-signal-mode-tabs {
  align-items: center;
  background: var(--color-neutral-100);
  border-radius: var(--radius-md);
  display: inline-flex;
  overflow: hidden;
  padding: 0.125rem;
}

.direct-signal-mode-tabs__button {
  border-radius: var(--radius-md);
  font-size: var(--text-xs);
  line-height: 1rem;
  padding: 0.25rem 0.75rem;
  transition: background-color 150ms ease, box-shadow 150ms ease, color 150ms ease;
  user-select: none;
}

.direct-signal-mode-tabs__button.is-active {
  background: var(--color-neutral-200);
  box-shadow: 0 0 0 1px var(--color-neutral-300);
  color: var(--color-neutral-900);
}

.direct-signal-mode-tabs__button.is-idle {
  color: var(--color-neutral-600);
}

.direct-signal-mode-tabs__button.is-idle:hover {
  background: color-mix(in srgb, var(--color-neutral-200) 50%, transparent);
}

.direct-signal-mode-tabs__button:disabled {
  opacity: 0.65;
}

.dark .direct-signal-mode-tabs {
  background: var(--color-neutral-800);
}

.dark .direct-signal-mode-tabs__button.is-active {
  background: var(--color-neutral-700);
  box-shadow: 0 0 0 1px var(--color-neutral-600);
  color: var(--color-neutral-50);
}

.dark .direct-signal-mode-tabs__button.is-idle {
  color: var(--color-neutral-300);
}

.dark .direct-signal-mode-tabs__button.is-idle:hover {
  background: color-mix(in srgb, var(--color-neutral-700) 50%, transparent);
}
</style>
