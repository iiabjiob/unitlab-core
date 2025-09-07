<template>
  <svg
    :width="size"
    :height="size"
    viewBox="0 0 24 24"
    fill="none"
    stroke-width="1.5"
    stroke="currentColor"
    xmlns="http://www.w3.org/2000/svg"
    :class="cls"
  >
    <path
      fill-rule="evenodd"
      clip-rule="evenodd"
      d="M19.5 8.25l-7.5 7.5-7.5-7.5"
    />
  </svg>
</template>

<script setup lang="ts">
defineProps<{
  size?: number | string
  cls?: string
}>()
</script>
