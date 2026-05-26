<template>
  <div class="u-tabs">
    <div class="u-tabs__list">
      <button
        v-for="(tab, index) in tabs"
        :key="index"
        class="u-tabs__button"
        :class="{ 'u-tabs__button--active': index === activeIndex }"
        type="button"
        @click="select(index)"
      >
        {{ tab.label }}
      </button>
    </div>
    <div class="u-tabs__panel">
      <slot :name="`tab-${activeIndex}`" />
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"

type TabDefinition = {
  label: string
}

const props = defineProps<{
  tabs: TabDefinition[]
  activeIndex: number
}>()

const emit = defineEmits<{
  (event: "update:activeIndex", value: number): void
}>()

const model = computed({
  get: () => props.activeIndex,
  set: value => emit("update:activeIndex", value),
})

type TabSlots = {
  [K in `tab-${number}`]?: () => unknown
}

defineSlots<TabSlots>()

function select(index: number) {
  if (index !== props.activeIndex) {
    model.value = index
  }
}
</script>

<style scoped>
.u-tabs {
  display: flex;
  flex-direction: column;
  height: 100%;
}

.u-tabs__list {
  align-items: center;
  background: var(--color-neutral-50);
  border-bottom: 1px solid var(--color-neutral-200);
  display: flex;
  font-size: var(--text-sm);
  font-weight: 500;
  gap: 0.5rem;
  line-height: 1.25rem;
  padding: 0.25rem 0.5rem;
}

.u-tabs__button {
  border-radius: var(--radius-md);
  color: var(--color-neutral-500);
  padding: 0.25rem 0.75rem;
  transition: background-color 150ms ease, color 150ms ease;
}

.u-tabs__button:hover {
  color: var(--color-neutral-900);
}

.u-tabs__button--active {
  background: var(--color-neutral-900);
  color: var(--color-white);
}

.u-tabs__panel {
  flex: 1 1 auto;
  overflow: hidden;
}

.dark .u-tabs__list {
  background: color-mix(in srgb, var(--color-neutral-900) 40%, transparent);
  border-color: var(--color-neutral-800);
}

.dark .u-tabs__button {
  color: var(--color-neutral-400);
}

.dark .u-tabs__button:hover {
  color: var(--color-neutral-100);
}

.dark .u-tabs__button--active {
  background: color-mix(in srgb, var(--color-white) 90%, transparent);
  color: var(--color-neutral-900);
}
</style>
