<template>
  <component
    :is="interactive ? 'button' : 'div'"
    :type="interactive ? 'button' : undefined"
    class="global-progress-card"
    :class="[
      compact ? 'global-progress-card--compact' : 'global-progress-card--regular',
      interactive ? 'global-progress-card--interactive' : '',
    ]"
    :title="compact ? (detail || label) : undefined"
    :aria-label="interactive ? (detail || label) : undefined"
    @click="handleClick"
  >
    <template v-if="compact">
      <span class="global-progress-card__dot" :class="dotClass"></span>
      <span class="global-progress-card__value">{{ percent }}%</span>
    </template>

    <template v-else>
      <div class="global-progress-card__body">
        <div class="global-progress-card__row">
          <span class="global-progress-card__value">{{ label }}</span>
          <span class="global-progress-card__value">{{ percent }}%</span>
        </div>
        <div class="global-progress-card__detail">{{ detail }}</div>
        <div class="global-progress-card__track">
          <div
            class="global-progress-card__bar"
            :class="barClass"
            :style="{ width: `${percent}%` }"
          ></div>
        </div>
      </div>
    </template>

    <slot name="actions" />
  </component>
</template>

<script setup lang="ts">
const props = withDefaults(defineProps<{
  compact?: boolean
  interactive?: boolean
  label: string
  percent: number
  detail: string
  dotClass?: string
  barClass?: string
}>(), {
  compact: false,
  interactive: false,
  dotClass: "global-progress-card__tone--success",
  barClass: "global-progress-card__tone--success",
})

const emit = defineEmits<{
  (event: "click"): void
}>()

function handleClick() {
  if (!props.interactive) {
    return
  }
  emit("click")
}
</script>

<style scoped>
.global-progress-card {
  display: inline-flex;
  align-items: center;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-neutral-50) 80%, transparent);
  color: var(--color-neutral-600);
}

button.global-progress-card {
  appearance: none;
  font: inherit;
}

.global-progress-card--compact {
  gap: 0.25rem;
  padding: 0.25rem 0.375rem;
  font-size: 10px;
  line-height: 1.2;
}

.global-progress-card--regular {
  gap: 0.5rem;
  padding: 0.25rem 0.5rem;
  font-size: 11px;
  line-height: 1.25;
}

.global-progress-card__dot {
  width: 0.375rem;
  height: 0.375rem;
  flex: 0 0 0.375rem;
  border-radius: 9999px;
}

.global-progress-card__value {
  color: var(--color-neutral-700);
  font-weight: 500;
}

.global-progress-card__body {
  min-width: 240px;
  max-width: 360px;
}

.global-progress-card__row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5rem;
  font-size: 11px;
}

.global-progress-card__detail {
  margin-top: 0.125rem;
  overflow: hidden;
  color: var(--color-neutral-600);
  font-size: 10px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.global-progress-card__track {
  height: 0.375rem;
  margin-top: 0.25rem;
  overflow: hidden;
  border-radius: var(--radius-sm);
  background: var(--color-neutral-200);
}

.global-progress-card__bar {
  height: 100%;
  transition: width 200ms ease;
}

.global-progress-card__tone--success {
  background: var(--color-emerald-500);
}

.global-progress-card__tone--warning {
  background: var(--color-amber-500);
}

.global-progress-card__tone--info {
  background: var(--color-sky-500);
}

:global(.dark .global-progress-card){
  border-color: var(--color-neutral-700);
  background: color-mix(in srgb, var(--color-neutral-800) 60%, transparent);
  color: var(--color-neutral-200);
}

:global(.dark .global-progress-card__value){
  color: var(--color-neutral-100);
}

:global(.dark .global-progress-card__detail){
  color: var(--color-neutral-300);
}

:global(.dark .global-progress-card__track){
  background: var(--color-neutral-700);
}
</style>
