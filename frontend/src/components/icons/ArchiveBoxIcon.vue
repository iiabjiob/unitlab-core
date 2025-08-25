<template>
  <svg
    :width="size"
    :height="size"
    viewBox="0 0 20 20"
    fill="currentColor"
    xmlns="http://www.w3.org/2000/svg"
    :class="cls"
  >
    <path d="M2 3a1 1 0 00-1 1v1a1 1 0 001 1h16a1 1 0 001-1V4a1 1 0 00-1-1H2z" />
    <path
      fill-rule="evenodd"
      clip-rule="evenodd"
      d="M2 7.5h16l-.811 7.71a2 2 0 01-1.99 1.79H4.802a2 2 0 01-1.99-1.79L2 7.5zM7 11a1 1 0 011-1h4a1 1 0 110 2H8a1 1 0 01-1-1z"
    />
  </svg>
</template>

<script setup lang="ts">
defineProps<{
  size?: number | string
  cls?: string
}>()
</script>
