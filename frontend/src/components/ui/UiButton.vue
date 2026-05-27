<template>
  <button
    :class="computedClass"
    :disabled="disabled"
    :type="type"
    v-bind="$attrs"
  >
    <slot />
  </button>
</template>

<script setup lang="ts">
import { computed } from "vue"

type ButtonVariant =
  | "primary"
  | "secondary"
  | "success"
  | "danger"
  | "toolbar"
  | "dashed"
  | "icon"
  | "ghost"

type ButtonSize = "xs" | "sm" | "base" | "lg"
type ButtonType = "button" | "submit" | "reset"

const props = withDefaults(
  defineProps<{
    variant?: ButtonVariant
    size?: ButtonSize
    type?: ButtonType
    disabled?: boolean
    full?: boolean
  }>(),
  {
    variant: "primary",
    size: "base",
    type: "button",
    disabled: false,
    full: false,
  }
)

const computedClass = computed(() => {
  const base = "btn"

  const variants: Record<ButtonVariant, string> = {
    primary: "btn-primary",
    secondary: "btn-secondary",
    success: "btn-success",
    danger: "btn-danger",
    toolbar: "btn-toolbar",
    dashed: "btn-dashed",
    icon: "btn-icon",
    ghost: "btn-ghost",
  }

  const sizes: Record<ButtonSize, string> = {
    xs: "btn-xs",
    sm: "btn-sm",
    base: "btn-base",
    lg: "btn-lg",
  }

  const fullClass = props.full ? "btn-full" : ""

  return `${base} ${variants[props.variant]} ${sizes[props.size]} ${fullClass}`
})
</script>
