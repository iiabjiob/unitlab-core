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
    <path stroke-linecap="round" stroke-linejoin="round" d="M6.75 12a.75.75 0 11-1.5 0 .75.75 0 011.5 0zM12.75 12a.75.75 0 11-1.5 0 .75.75 0 011.5 0zM18.75 12a.75.75 0 11-1.5 0 .75.75 0 011.5 0z" />
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
