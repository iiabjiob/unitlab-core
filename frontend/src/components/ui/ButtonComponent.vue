<template>
  <button :class="computedClass" :disabled="disabled">
    <slot />
  </button>
</template>

<script setup>
import { computed } from "vue";

const props = defineProps({
  type: { type: String, default: "primary" }, // primary, secondary, danger, outline-primary, outline-secondary, outline-danger
  size: { type: String, default: "base" }, // xs, sm, base, lg
  disabled: { type: Boolean, default: false },
});

const computedClass = computed(() => {
  const base = "btn";

  const types = {
    primary: "btn-primary",
    secondary: "btn-secondary",
    danger: "btn-danger",
  };

  const sizes = {
    xs: "btn-xs",
    sm: "btn-sm",
    base: "btn-base",
    lg: "btn-lg",
  };

  return `${base} ${types[props.type] || types.primary} ${sizes[props.size] || sizes.base}`;
});
</script>
