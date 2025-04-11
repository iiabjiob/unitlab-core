<template>
  <button :class="computedClass" :disabled="disabled">
    <slot />
  </button>
</template>

<script setup lang="ts">
import { computed } from 'vue'

type ButtonType = 'primary' | 'secondary' | 'danger'
type ButtonSize = 'xs' | 'sm' | 'base' | 'lg'

const props = withDefaults(
  defineProps<{
    type?: ButtonType
    size?: ButtonSize
    disabled?: boolean
  }>(),
  {
    type: 'primary',
    size: 'base',
    disabled: false
  }
)

const computedClass = computed(() => {
  const base = 'btn'

  const types: Record<ButtonType, string> = {
    primary: 'btn-primary',
    secondary: 'btn-secondary',
    danger: 'btn-danger',
  }

  const sizes: Record<ButtonSize, string> = {
    xs: 'btn-xs',
    sm: 'btn-sm',
    base: 'btn-base',
    lg: 'btn-lg'
  }

  return `${base} ${types[props.type]} ${sizes[props.size]}`
})
</script>
