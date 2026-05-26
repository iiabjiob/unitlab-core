<template>
  <div :class="['loading-spinner', positionClass, customClass]">
    <svg :class="['loading-spinner__icon', sizeClass]" :style="iconStyle" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="4">
      <circle cx="12" cy="12" r="10" stroke-opacity="0.3"></circle>
      <path d="M12 2a10 10 0 0 1 10 10" stroke-linecap="round"></path>
    </svg>
    <p v-if="text" class="loading-spinner__text">{{ text }}</p>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'

interface SpinnerProps {
  text?: string
  color?: string
  size?: 'small' | 'medium' | 'large'
  position?: 'left' | 'center' | 'right'
  customClass?: string
}

const props = defineProps<SpinnerProps>()

const sizeClass = computed(() => {
  switch (props.size) {
    case 'small':
      return 'loading-spinner__icon--small'
    case 'large':
      return 'loading-spinner__icon--large'
    default:
      return 'loading-spinner__icon--medium'
  }
})

const positionClass = computed(() => {
  switch (props.position) {
    case 'left':
      return 'loading-spinner--left'
    case 'right':
      return 'loading-spinner--right'
    default:
      return 'loading-spinner--center'
  }
})

const iconStyle = computed(() => ({
  color: resolveSpinnerColor(props.color),
}))

function resolveSpinnerColor(color: string | undefined): string {
  switch (color) {
    case 'blue':
      return 'var(--color-blue-500)'
    case 'green':
      return 'var(--color-green-600)'
    case 'red':
      return 'var(--color-red-500)'
    case 'yellow':
      return 'var(--color-yellow-600)'
    default:
      return 'var(--color-neutral-500)'
  }
}
</script>

<style scoped>
.loading-spinner {
  align-items: center;
  display: flex;
}

.loading-spinner--left {
  justify-content: flex-start;
}

.loading-spinner--center {
  justify-content: center;
}

.loading-spinner--right {
  justify-content: flex-end;
}

.loading-spinner__icon {
  animation: loading-spinner-rotate 1s linear infinite;
  flex: none;
}

.loading-spinner__icon--small {
  height: 1.25rem;
  width: 1.25rem;
}

.loading-spinner__icon--medium {
  height: 2rem;
  width: 2rem;
}

.loading-spinner__icon--large {
  height: 3rem;
  width: 3rem;
}

.loading-spinner__text {
  color: var(--color-neutral-500);
  margin: 0 0 0 0.5rem;
}

@keyframes loading-spinner-rotate {
  to {
    transform: rotate(360deg);
  }
}
</style>
