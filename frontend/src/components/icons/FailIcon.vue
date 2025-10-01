<template>
  <svg
    xmlns="http://www.w3.org/2000/svg"
    :width="size"
    :height="size"
    viewBox="0 0 20 20"
    fill="currentColor"
    :class="cls"
  >
    <path
      fill-rule="evenodd"
      clip-rule="evenodd"
      d="M33.707,8,40.854.854a.5.5,0,0,0-.708-.708L33,7.293,25.854.146a.5.5,0,0,0-.708.708L32.293,8l-7.147,7.146a.5.5,0,0,0,.708.708L33,8.707l7.146,7.147a.5.5,0,0,0,.708-.708Z"
      transform="translate(-23 2)"
    />
  </svg>
</template>

<script setup lang="ts">
defineProps<{
  size?: number | string
  cls?: string
}>()
</script>
