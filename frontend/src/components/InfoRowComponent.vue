<template>
  <p class="flex flex-wrap items-center gap-x-2">
    <span><strong>{{ label }}:</strong></span>
    <span class="text-gray-500">
      <slot></slot> <!-- Значение передаётся через слот -->
    </span>
  </p>
</template>

<script setup>
defineProps({
  label: String
});
</script>
