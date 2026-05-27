<template>
  <div
    v-if="!appReady"
    class="app-bootstrap-placeholder"
    role="status"
    aria-live="polite"
    aria-busy="true"
  >
    <div class="app-bootstrap-placeholder__card">
      <div class="app-bootstrap-placeholder__headline">UnitLab</div>
      <div class="app-bootstrap-placeholder__chip">
        <span class="app-bootstrap-placeholder__spinner" aria-hidden="true"></span>
        <span>Loading…</span>
      </div>
    </div>
  </div>
  <AppLayout v-else />
  <ToastContainer />
</template>

<script setup lang="ts">
import { onMounted, ref } from "vue"
import AppLayout from "./components/layout/AppLayout.vue"
import ToastContainer from "./components/ui/ToastContainer.vue"
import router from "./router"

const appReady = ref(false)

onMounted(async () => {
  await router.isReady()
  appReady.value = true
})
</script>

<style scoped>
.app-bootstrap-placeholder {
  position: fixed;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 2000;
  background: color-mix(in srgb, var(--color-neutral-50) 85%, transparent);
  color: var(--color-neutral-700);
}

.app-bootstrap-placeholder__card {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.5rem;
  min-width: 16rem;
  padding: 1rem 1.25rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 80%, transparent);
  border-radius: 0.9rem;
  background: color-mix(in srgb, var(--color-white) 95%, transparent);
  box-shadow: var(--shadow-sm);
}

.app-bootstrap-placeholder__headline {
  font-size: 0.95rem;
  font-weight: 700;
  line-height: 1.25rem;
}

.app-bootstrap-placeholder__description {
  max-width: 24rem;
  text-align: center;
  font-size: 0.78rem;
  line-height: 1.1rem;
  opacity: 0.82;
}

.app-bootstrap-placeholder__chip {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  font-size: 0.85rem;
  font-weight: 600;
  border-radius: 0.75rem;
}

.app-bootstrap-placeholder__spinner {
  width: 0.9rem;
  height: 0.9rem;
  border-radius: 9999px;
  border: 2px solid currentColor;
  border-right-color: transparent;
  animation: app-bootstrap-spin 0.9s linear infinite;
}

:global(.dark) .app-bootstrap-placeholder {
  background: color-mix(in srgb, var(--color-neutral-950) 90%, transparent);
  color: var(--color-neutral-200);
}

:global(.dark) .app-bootstrap-placeholder__card {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-950) 95%, transparent);
}

@keyframes app-bootstrap-spin {
  to {
    transform: rotate(360deg);
  }
}
</style>
