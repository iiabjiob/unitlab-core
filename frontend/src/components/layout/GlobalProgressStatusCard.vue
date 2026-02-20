<template>
  <component
    :is="interactive ? 'button' : 'div'"
    :type="interactive ? 'button' : undefined"
    class="inline-flex items-center rounded-md border border-neutral-200 bg-neutral-50/80 text-neutral-600 dark:border-neutral-700 dark:bg-neutral-800/60 dark:text-neutral-200"
    :class="compact ? 'gap-1 px-1.5 py-1 text-[10px]' : 'gap-2 px-2 py-1 text-[11px]'"
    :title="compact ? (detail || label) : undefined"
    :aria-label="interactive ? (detail || label) : undefined"
    @click="handleClick"
  >
    <template v-if="compact">
      <span class="h-1.5 w-1.5 rounded-full" :class="dotClass"></span>
      <span class="font-medium text-neutral-700 dark:text-neutral-100">{{ percent }}%</span>
    </template>

    <template v-else>
      <div class="min-w-[240px] max-w-[360px]">
        <div class="flex items-center justify-between gap-2 text-[11px]">
          <span class="font-medium text-neutral-700 dark:text-neutral-100">{{ label }}</span>
          <span class="font-medium text-neutral-700 dark:text-neutral-100">{{ percent }}%</span>
        </div>
        <div class="mt-0.5 truncate text-[10px] text-neutral-600 dark:text-neutral-300">{{ detail }}</div>
        <div class="mt-1 h-1.5 overflow-hidden rounded bg-neutral-200 dark:bg-neutral-700">
          <div
            class="h-full transition-[width] duration-200"
            :class="barClass"
            :style="{ width: `${percent}%` }"
          ></div>
        </div>
      </div>
    </template>

    <slot name="actions" />
  </component>
</template>

<script setup lang="ts">
const props = withDefaults(defineProps<{
  compact?: boolean
  interactive?: boolean
  label: string
  percent: number
  detail: string
  dotClass?: string
  barClass?: string
}>(), {
  compact: false,
  interactive: false,
  dotClass: "bg-emerald-500",
  barClass: "bg-emerald-500",
})

const emit = defineEmits<{
  (event: "click"): void
}>()

function handleClick() {
  if (!props.interactive) {
    return
  }
  emit("click")
}
</script>
