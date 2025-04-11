<template>
  <div :class="['flex items-center', positionClass, customClass]">
    <svg :class="['animate-spin', sizeClass, colorClass]" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="4">
      <circle cx="12" cy="12" r="10" stroke-opacity="0.3"></circle>
      <path d="M12 2a10 10 0 0 1 10 10" stroke-linecap="round"></path>
    </svg>
    <p v-if="text" class="ml-2 text-gray-500">{{ text }}</p>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'

// ✅ Явно определяем интерфейс Props
interface SpinnerProps {
  text?: string
  color?: string
  size?: 'small' | 'medium' | 'large'
  position?: 'left' | 'center' | 'right'
  customClass?: string
}

const props = defineProps<SpinnerProps>()

// ✅ Классы цвета, размера и позиции с тайпсейфти
const colorClass = computed(() => `text-${props.color ?? 'gray'}-500`)

const sizeClass = computed(() => {
  switch (props.size) {
    case 'small':
      return 'h-5 w-5'
    case 'large':
      return 'h-12 w-12'
    default:
      return 'h-8 w-8' // medium (или undefined)
  }
})

const positionClass = computed(() => {
  switch (props.position) {
    case 'left':
      return 'justify-start'
    case 'right':
      return 'justify-end'
    default:
      return 'justify-center'
  }
})
</script>
