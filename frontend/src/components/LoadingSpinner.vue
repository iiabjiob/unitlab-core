<template>
  <div :class="['flex items-center', positionClass, customClass]">
    <svg :class="['animate-spin', sizeClass, colorClass]" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="4">
      <circle cx="12" cy="12" r="10" stroke-opacity="0.3"></circle>
      <path d="M12 2a10 10 0 0 1 10 10" stroke-linecap="round"></path>
    </svg>
    <p v-if="text" class="ml-2 text-gray-500">{{ text }}</p>
  </div>
</template>

<script setup>
import { computed } from "vue";

const props = defineProps({
  text: {
    type: String,
    default: "Loading...",
  },
  color: {
    type: String,
    default: "emerald", // Цвет по умолчанию
  },
  size: {
    type: String,
    default: "medium", // Размер по умолчанию
    validator: (value) => ["small", "medium", "large"].includes(value),
  },
  position: {
    type: String,
    default: "left",
    validator: (value) => ["center", "left", "right"].includes(value),
  },
  customClass: {
    type: String,
    default: "",
  },
});

// Определяем классы для цвета, размера и позиции
const colorClass = computed(() => `text-${props.color}-500`);
const sizeClass = computed(() => {
  return props.size === "small" ? "h-5 w-5" :
         props.size === "large" ? "h-12 w-12" :
         "h-8 w-8"; // Default (medium)
});
const positionClass = computed(() => {
  return props.position === "left" ? "justify-start" :
         props.position === "right" ? "justify-end" :
         "justify-center"; // Default (center)
});
</script>
