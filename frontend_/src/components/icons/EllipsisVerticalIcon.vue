<template>
  <svg
    :width="props.size"
    :height="props.size"
    viewBox="0 0 24 24"
    :fill="props.fill"
    xmlns="http://www.w3.org/2000/svg"
    v-bind="$attrs"
    class="inline-block"
  >
    <path stroke-linecap="round" stroke-linejoin="round" d="M12 6.75a.75.75 0 1 1 0-1.5.75.75 0 0 1 0 1.5ZM12 12.75a.75.75 0 1 1 0-1.5.75.75 0 0 1 0 1.5ZM12 18.75a.75.75 0 1 1 0-1.5.75.75 0 0 1 0 1.5Z" />
  </svg>
</template>

<script setup lang="ts">
const props = withDefaults(defineProps<{
  size?: number | string
  fill?: string
}>(), {
  size: 20,
  fill: "currentColor"
})
</script>
