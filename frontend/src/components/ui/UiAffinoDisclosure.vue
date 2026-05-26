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
  containerClass: "ui-affino-disclosure",
  headerClass: "ui-affino-disclosure__header",
  contentClass: "",
})

const disclosure = useDisclosureController(Boolean(props.defaultOpen))
const isOpen = computed(() => disclosure.state.value.open)
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
      <span>{{ title }}</span>
      <span>{{ isOpen ? "▾" : "▸" }}</span>
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
  justify-content: space-between;
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

.dark .ui-affino-disclosure {
  border-color: var(--color-neutral-700);
}

.dark .ui-affino-disclosure__header {
  color: var(--color-neutral-400);
}
</style>
