<template>
  <div v-if="props.message" :class="['alert', `alert-${props.type}`]">
    <span v-if="props.icon">{{ props.icon }}</span>
    <span>{{ props.message }}</span>
  </div>
</template>

<script setup lang="ts">
const props = withDefaults(
  defineProps<{
    message: string
    type?: 'info' | 'warning' | 'success' | 'error' | string
    icon?: string
  }>(),
  {
    type: 'info',
    icon: ''
  }
)
</script>
