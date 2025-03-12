<template>
  <div v-if="message" :class="alertClass">
    <span v-if="icon" class="mr-2">{{ icon }}</span>
    <span>{{ message }}</span>
  </div>
</template>

<script setup>
import { computed } from "vue";

const props = defineProps({
  message: String,
  type: {
    type: String,
    default: "info", // error | warning | success | info
  },
  icon: {
    type: String,
    default: "", // Можно передавать эмодзи или SVG-иконку
  },
});

const alertClass = computed(() => {
  return {
    "text-red-700 dark:text-red-300": props.type === "error",
    "text-yellow-700 dark:text-yellow-300": props.type === "warning",
    "text-blue-700 dark:text-blue-300": props.type === "info",
    "text-green-700 dark:text-green-300": props.type === "success",
  };
});
</script>
