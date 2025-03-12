<template>
  <button :class="computedClass" :disabled="disabled">
    <slot />
  </button>
</template>

<script setup>
import { computed } from "vue";

const props = defineProps({
  type: { type: String, default: "primary" }, // primary, secondary, danger
  disabled: { type: Boolean, default: false },
});

const computedClass = computed(() => {
  const base = "btn";

  const types = {
    primary: "btn-primary",
    secondary: "btn-secondary",
    danger: "btn-danger",
  };

  return `${base} ${types[props.type] || types.primary} ${props.disabled ? "opacity-50 cursor-not-allowed" : ""}`;
});
</script>
