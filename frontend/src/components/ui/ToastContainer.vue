<template>
  <div
    v-for="position in positions"
    :key="position"
    class="toast-stack"
    :class="positionClass(position)"
  >
    <transition-group name="toast" tag="div" class="toast-stack__group">
      <div
        v-for="toast in toastStore.getByPosition(position)"
        :key="toast.id"
        class="toast-card"
        :class="variantFrameClass(toast.variant)"
        role="status"
        aria-live="polite"
      >
        <div class="toast-card__row">
          <div class="toast-card__body">
            <span
              class="toast-card__badge"
              :class="variantBadgeClass(toast.variant)"
            >
              {{ variantLabel(toast.variant) }}
            </span>
            <p class="toast-card__message">
              {{ toast.message }}
            </p>
          </div>
          <button
            v-if="toast.actionLabel && toast.onAction"
            type="button"
            class="btn btn-xs btn-primary toast-card__button"
            :aria-label="toast.actionLabel"
            @click="runAction(toast.id, toast.onAction)"
          >
            {{ toast.actionLabel }}
          </button>
          <button
            type="button"
            class="btn btn-xs btn-secondary toast-card__button"
            aria-label="Dismiss notification"
            @click="remove(toast.id)"
          >
            Dismiss
          </button>
        </div>
      </div>
    </transition-group>
  </div>
</template>

<script setup lang="ts">
import { useToastStore, type ToastVariant, type ToastPosition } from "@/stores/toastStore"

const toastStore = useToastStore()

const positions: ToastPosition[] = [
  "top-left",
  "top-right",
  "bottom-left",
  "bottom-right",
  "top-center",
  "bottom-center",
]

function remove(id: number) {
  toastStore.remove(id)
}

function runAction(id: number, action: () => void) {
  action()
  toastStore.remove(id)
}

function variantLabel(variant: ToastVariant) {
  switch (variant) {
    case "success":
      return "ok"
    case "error":
      return "error"
    default:
      return "info"
  }
}

function variantFrameClass(variant: ToastVariant) {
  switch (variant) {
    case "success":
      return "toast-card--success"
    case "error":
      return "toast-card--error"
    default:
      return "toast-card--info"
  }
}

function variantBadgeClass(variant: ToastVariant) {
  switch (variant) {
    case "success":
      return "toast-card__badge--success"
    case "error":
      return "toast-card__badge--error"
    default:
      return "toast-card__badge--info"
  }
}

function positionClass(pos: ToastPosition) {
  switch (pos) {
    case "top-left":
      return "toast-stack--top-left"
    case "top-right":
      return "toast-stack--top-right"
    case "bottom-left":
      return "toast-stack--bottom-left"
    case "bottom-right":
      return "toast-stack--bottom-right"
    case "top-center":
      return "toast-stack--top-center"
    case "bottom-center":
      return "toast-stack--bottom-center"
  }
  return ""
}
</script>

<style scoped>
.toast-stack {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  padding: 1rem;
  pointer-events: none;
  position: fixed;
  z-index: 50;
}

.toast-stack__group {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  max-width: 24rem;
  width: 100%;
}

.toast-stack--top-left {
  align-items: flex-start;
  left: 1rem;
  top: 1rem;
}

.toast-stack--top-right {
  align-items: flex-end;
  right: 1rem;
  top: 1rem;
}

.toast-stack--bottom-left {
  align-items: flex-start;
  bottom: 1rem;
  left: 1rem;
}

.toast-stack--bottom-right {
  align-items: flex-end;
  bottom: 1rem;
  right: 1rem;
}

.toast-stack--top-center {
  align-items: center;
  left: 50%;
  top: 1rem;
  transform: translateX(-50%);
}

.toast-stack--bottom-center {
  align-items: center;
  bottom: 1rem;
  left: 50%;
  transform: translateX(-50%);
}

.toast-card {
  backdrop-filter: blur(4px);
  background: color-mix(in srgb, var(--color-neutral-50) 95%, transparent);
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 80%, transparent);
  border-radius: var(--radius-md);
  box-shadow: 0 4px 6px -1px rgb(23 23 23 / 0.1), 0 2px 4px -2px rgb(23 23 23 / 0.1);
  color: var(--color-neutral-900);
  padding: 0.625rem;
  pointer-events: auto;
}

.toast-card__row {
  align-items: flex-start;
  display: flex;
  gap: 0.75rem;
  justify-content: space-between;
}

.toast-card__body {
  align-items: flex-start;
  display: flex;
  gap: 0.625rem;
  min-width: 0;
}

.toast-card__badge {
  align-items: center;
  border: 1px solid;
  border-radius: 0.25rem;
  display: inline-flex;
  flex-shrink: 0;
  font-size: 0.625rem;
  font-weight: 500;
  letter-spacing: 0.06em;
  line-height: 1rem;
  margin-top: 0.125rem;
  padding: 0.125rem 0.375rem;
  text-transform: uppercase;
}

.toast-card__badge--success {
  background: color-mix(in srgb, var(--color-emerald-600) 10%, transparent);
  border-color: color-mix(in srgb, var(--color-emerald-700) 25%, transparent);
  color: var(--color-emerald-700);
}

.toast-card__badge--error {
  background: color-mix(in srgb, var(--color-rose-600) 10%, transparent);
  border-color: color-mix(in srgb, var(--color-rose-700) 25%, transparent);
  color: var(--color-rose-700);
}

.toast-card__badge--info {
  background: color-mix(in srgb, var(--color-neutral-500) 10%, transparent);
  border-color: color-mix(in srgb, var(--color-neutral-500) 25%, transparent);
  color: var(--color-neutral-700);
}

.toast-card__message {
  color: var(--color-neutral-800);
  font-size: var(--text-xs);
  line-height: 1rem;
  margin: 0;
}

.toast-card__button {
  flex-shrink: 0;
}

.dark .toast-card {
  background: color-mix(in srgb, var(--color-neutral-900) 95%, transparent);
  border-color: var(--color-neutral-700);
  box-shadow: 0 4px 6px -1px rgb(0 0 0 / 0.4), 0 2px 4px -2px rgb(0 0 0 / 0.4);
  color: var(--color-neutral-100);
}

.dark .toast-card__badge--success {
  background: color-mix(in srgb, var(--color-emerald-300) 10%, transparent);
  border-color: color-mix(in srgb, var(--color-emerald-300) 25%, transparent);
  color: var(--color-emerald-300);
}

.dark .toast-card__badge--error {
  background: color-mix(in srgb, var(--color-rose-300) 10%, transparent);
  border-color: color-mix(in srgb, var(--color-rose-300) 25%, transparent);
  color: var(--color-rose-300);
}

.dark .toast-card__badge--info {
  background: color-mix(in srgb, var(--color-neutral-300) 10%, transparent);
  border-color: color-mix(in srgb, var(--color-neutral-300) 25%, transparent);
  color: var(--color-neutral-200);
}

.dark .toast-card__message {
  color: var(--color-neutral-100);
}

.toast-enter-active,
.toast-leave-active {
  transition: all 0.3s ease;
}
.toast-enter-from,
.toast-leave-to {
  opacity: 0;
  transform: translateY(-10px);
}
</style>
