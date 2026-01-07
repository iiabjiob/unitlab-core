<script setup lang="ts">
const props = withDefaults(defineProps<{
  tag: string
  title: string
  description?: string
}>(), {
  description: "",
})
</script>

<template>
  <div class="flex h-full flex-col items-center justify-center rounded-2xl border border-dashed border-neutral-300/80 bg-white/80 px-6 py-16 text-center text-neutral-600 dark:border-neutral-700 dark:bg-neutral-900">
    <p class="text-[11px] uppercase tracking-[0.4em] text-neutral-400 dark:text-neutral-500">
      {{ props.tag }}
    </p>
    <h2 class="mt-4 text-2xl font-semibold text-neutral-900 dark:text-neutral-100">
      {{ props.title }}
    </h2>
    <p v-if="props.description" class="mt-2 max-w-md text-sm text-neutral-500 dark:text-neutral-400">
      {{ props.description }}
    </p>
    <slot />
  </div>
</template>
