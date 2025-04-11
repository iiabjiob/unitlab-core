<template>
  <p class="flex flex-wrap items-center gap-x-2">
    <span class="font-semibold">{{ label }}:</span>
    <span class="text-gray-500">
      <slot></slot> <!-- Значение передаётся через слот -->
    </span>
  </p>
</template>

<script setup lang="ts">
defineProps({
  label: String
});
</script>
