<script setup lang="ts">
import { computed } from "vue"
import { useDisclosureController } from "@affino/disclosure-vue"

const props = withDefaults(defineProps<{
  title: string
  defaultOpen?: boolean
  containerClass?: string
  headerClass?: string
  contentClass?: string
  statusLabel?: string | null
  statusTone?: "neutral" | "success" | "warning" | "danger"
  metaLabels?: readonly string[]
}>(), {
  defaultOpen: true,
  containerClass: "ui-affino-disclosure",
  headerClass: "ui-affino-disclosure__header",
  contentClass: "",
  statusLabel: null,
  statusTone: "neutral",
  metaLabels: () => [],
})

const disclosure = useDisclosureController(Boolean(props.defaultOpen))
const isOpen = computed(() => disclosure.state.value.open)
const hasMetaLabels = computed(() => props.metaLabels.length > 0)

const statusToneClass = computed(() => {
  switch (props.statusTone) {
    case "success":
      return "ui-affino-disclosure__status--success"
    case "warning":
      return "ui-affino-disclosure__status--warning"
    case "danger":
      return "ui-affino-disclosure__status--danger"
    default:
      return "ui-affino-disclosure__status--neutral"
  }
})
</script>

<template>
  <div :class="containerClass">
    <button
      type="button"
      class="ui-affino-disclosure__button"
      :class="headerClass"
      :aria-expanded="isOpen"
      @click="disclosure.toggle()"
    >
      <span class="ui-affino-disclosure__heading">
        <span class="ui-affino-disclosure__title-row">
          <span class="ui-affino-disclosure__title">{{ title }}</span>
          <span
            v-if="statusLabel"
            class="ui-affino-disclosure__status"
            :class="statusToneClass"
          >
            {{ statusLabel }}
          </span>
        </span>
        <span
          v-if="hasMetaLabels"
          class="ui-affino-disclosure__meta-row"
        >
          <span
            v-for="metaLabel in metaLabels"
            :key="metaLabel"
            class="ui-affino-disclosure__meta-badge"
          >
            {{ metaLabel }}
          </span>
        </span>
      </span>
      <span class="ui-affino-disclosure__chevron">
        {{ isOpen ? "▾" : "▸" }}
      </span>
    </button>

    <div v-if="isOpen" :class="contentClass">
      <slot :open="isOpen" />
    </div>
  </div>
</template>

<style scoped>
.ui-affino-disclosure {
  border: 1px solid var(--color-neutral-200);
  border-radius: 0.5rem;
  padding: 0.75rem;
}

.ui-affino-disclosure__button {
  align-items: center;
  display: flex;
  gap: 0.75rem;
  justify-content: space-between;
  text-align: left;
  width: 100%;
}

.ui-affino-disclosure__header {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  letter-spacing: 0.025em;
  line-height: 1rem;
  margin-bottom: 0.5rem;
  text-transform: uppercase;
}

.ui-affino-disclosure__heading {
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  gap: 0.35rem;
  min-width: 0;
}

.ui-affino-disclosure__title-row {
  align-items: center;
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  min-width: 0;
}

.ui-affino-disclosure__title {
  color: inherit;
  min-width: 0;
  overflow-wrap: anywhere;
}

.ui-affino-disclosure__status {
  align-items: center;
  border: 1px solid transparent;
  border-radius: 999px;
  display: inline-flex;
  flex: 0 0 auto;
  font-size: var(--text-xs);
  font-weight: 600;
  line-height: 1;
  padding: 0.2rem 0.5rem;
  text-transform: none;
}

.ui-affino-disclosure__status--success {
  background: color-mix(in srgb, var(--color-emerald-50) 72%, var(--color-white));
  border-color: color-mix(in srgb, var(--color-emerald-200) 60%, var(--color-white));
  color: var(--color-emerald-800);
}

.ui-affino-disclosure__status--warning {
  background: color-mix(in srgb, var(--color-amber-50) 72%, var(--color-white));
  border-color: color-mix(in srgb, var(--color-amber-200) 60%, var(--color-white));
  color: var(--color-amber-800);
}

.ui-affino-disclosure__status--danger {
  background: color-mix(in srgb, var(--color-rose-50) 72%, var(--color-white));
  border-color: color-mix(in srgb, var(--color-rose-200) 60%, var(--color-white));
  color: var(--color-rose-800);
}

.ui-affino-disclosure__status--neutral {
  background: color-mix(in srgb, var(--color-neutral-100) 72%, var(--color-white));
  border-color: color-mix(in srgb, var(--color-neutral-200) 60%, var(--color-white));
  color: var(--color-neutral-700);
}

.ui-affino-disclosure__meta-row {
  display: flex;
  flex-wrap: wrap;
  gap: 0.35rem;
}

.ui-affino-disclosure__meta-badge {
  align-items: center;
  background: color-mix(in srgb, var(--color-neutral-100) 76%, var(--color-white));
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 68%, var(--color-white));
  border-radius: 999px;
  color: var(--color-neutral-600);
  display: inline-flex;
  font-size: var(--text-xs);
  font-weight: 500;
  line-height: 1;
  padding: 0.18rem 0.45rem;
}

.ui-affino-disclosure__chevron {
  flex: 0 0 auto;
}

:global(.dark .ui-affino-disclosure) {
  border-color: var(--color-neutral-700);
}

:global(.dark .ui-affino-disclosure__header) {
  color: var(--color-neutral-400);
}

:global(.dark .ui-affino-disclosure__status--success) {
  background: color-mix(in srgb, var(--color-emerald-950) 60%, var(--color-neutral-950));
  border-color: color-mix(in srgb, var(--color-emerald-800) 60%, var(--color-neutral-950));
  color: var(--color-emerald-200);
}

:global(.dark .ui-affino-disclosure__status--warning) {
  background: color-mix(in srgb, var(--color-amber-950) 60%, var(--color-neutral-950));
  border-color: color-mix(in srgb, var(--color-amber-800) 60%, var(--color-neutral-950));
  color: var(--color-amber-200);
}

:global(.dark .ui-affino-disclosure__status--danger) {
  background: color-mix(in srgb, var(--color-rose-950) 60%, var(--color-neutral-950));
  border-color: color-mix(in srgb, var(--color-rose-800) 60%, var(--color-neutral-950));
  color: var(--color-rose-200);
}

:global(.dark .ui-affino-disclosure__status--neutral) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-300);
}

:global(.dark .ui-affino-disclosure__meta-badge) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-300);
}
</style>
