<template>
  <div v-if="message" :class="['alert', `alert-${type}`]">
    <span v-if="icon">{{ icon }}</span>
    <span>{{ message }}</span>
  </div>
</template>

<script setup>
const { message, type, icon } = defineProps({
  message: String,
  type: {
    type: String,
    default: "info",
  },
  icon: {
    type: String,
    default: "",
  },
});
</script>
