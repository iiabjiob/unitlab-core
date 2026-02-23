<script setup lang="ts">
import { computed } from "vue"
import { useDisclosureController } from "@affino/disclosure-vue"

const props = withDefaults(defineProps<{
  title: string
  defaultOpen?: boolean
  containerClass?: string
  headerClass?: string
  contentClass?: string
}>(), {
  defaultOpen: true,
  containerClass: "rounded-lg border border-neutral-200 dark:border-neutral-700 p-3",
  headerClass: "mb-2 text-xs uppercase tracking-wide text-neutral-500 dark:text-neutral-400",
  contentClass: "",
})

const disclosure = useDisclosureController(Boolean(props.defaultOpen))
const isOpen = computed(() => disclosure.state.value.open)
</script>

<template>
  <div :class="containerClass">
    <button
      type="button"
      class="w-full flex items-center justify-between"
      :class="headerClass"
      :aria-expanded="isOpen"
      @click="disclosure.toggle()"
    >
      <span>{{ title }}</span>
      <span>{{ isOpen ? "▾" : "▸" }}</span>
    </button>

    <div v-if="isOpen" :class="contentClass">
      <slot :open="isOpen" />
    </div>
  </div>
</template>
