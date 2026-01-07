<script setup lang="ts">
import { computed } from "vue"

defineOptions({ inheritAttrs: false })

const props = withDefaults(defineProps<{
  active?: boolean
  disabled?: boolean
}>(), {
  active: false,
  disabled: false,
})

const emit = defineEmits<{ (e: "select"): void }>()

const rootClasses = computed(() => [
  "group",
  props.disabled ? "cursor-default opacity-60" : "cursor-pointer",
  "select-none flex items-center px-3 py-2 text-neutral-400 dark:text-neutral-600 hover:bg-neutral-850 transition-colors",
  props.active ? "text-neutral-900 dark:text-white" : null,
])

function handleClick(event: MouseEvent) {
  if (props.disabled) return
  emit("select")
}
</script>

<template>
  <div
    v-bind="$attrs"
    :class="rootClasses"
    @click="handleClick"
  >
    <div
      class="w-1 h-5 mr-2 rounded transition-colors"
      :class="props.active ? 'bg-blue-500' : 'bg-transparent group-hover:bg-neutral-700'"
    />

    <div class="flex-1 min-w-0">
      <div class="flex items-center gap-2 min-w-0">
        <slot name="prefix" />
        <div class="truncate flex-1 min-w-0">
          <slot />
        </div>
        <slot name="suffix" />
      </div>

      <div v-if="$slots.subtitle" class="text-[11px] uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400 mt-0.5">
        <slot name="subtitle" />
      </div>
      <div v-else-if="$slots.meta" class="text-xs text-neutral-500 dark:text-neutral-400 mt-0.5">
        <slot name="meta" />
      </div>
    </div>
  </div>
</template>
