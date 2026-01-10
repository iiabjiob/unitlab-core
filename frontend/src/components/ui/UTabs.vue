<template>
  <div class="flex h-full flex-col">
    <div class="flex items-center gap-2 border-b border-neutral-200 bg-neutral-50 px-2 py-1 text-sm font-medium dark:border-neutral-800 dark:bg-neutral-900/40">
      <button
        v-for="(tab, index) in tabs"
        :key="index"
        class="rounded-md px-3 py-1 transition"
        :class="index === activeIndex
          ? 'bg-neutral-900 text-white dark:bg-white/90 dark:text-neutral-900'
          : 'text-neutral-500 hover:text-neutral-900 dark:text-neutral-400 dark:hover:text-neutral-100'"
        type="button"
        @click="select(index)"
      >
        {{ tab.label }}
      </button>
    </div>
    <div class="flex-1 overflow-hidden">
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
