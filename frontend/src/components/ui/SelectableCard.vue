<template>
  <div
    class="selectable-card"
    :class="selected ? 'selectable-card--selected' : 'selectable-card--idle'"
    @click="$emit('select')"
  >
    <slot />
  </div>
</template>

<script setup lang="ts">
defineProps<{
  selected?: boolean
}>()

defineEmits<{ (e: "select"): void }>()
</script>
