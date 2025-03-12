<template>
  <div v-if="message" :class="alertClass" class="p-4 rounded-md text-sm flex items-center">
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
    "bg-red-100 text-red-700 border border-red-400": props.type === "error",
    "bg-yellow-100 text-yellow-700 border border-yellow-400": props.type === "warning",
    "bg-blue-100 text-blue-700 border border-blue-400": props.type === "info",
    "bg-green-100 text-green-700 border border-green-400": props.type === "success",
  };
});
</script>
