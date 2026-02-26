<template>
  <div
    v-if="!appReady"
    class="app-bootstrap-placeholder bg-neutral-50/85 text-neutral-700 dark:bg-neutral-950/75 dark:text-neutral-200"
    role="status"
    aria-live="polite"
    aria-busy="true"
  >
    <div class="app-bootstrap-placeholder__card border border-neutral-200/80 bg-white/95 px-5 py-4 shadow-sm dark:border-neutral-800 dark:bg-neutral-900/90">
      <img :src="logoUrl" alt="UnitLab" class="app-bootstrap-placeholder__logo" />
      <div class="app-bootstrap-placeholder__headline">UnitLab Core</div>
      <div class="app-bootstrap-placeholder__description">Industrial test and commissioning platform</div>
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
import logoUrl from "./assets/logo.svg"

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
}

.app-bootstrap-placeholder__card {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.5rem;
  min-width: 16rem;
  border-radius: 0.9rem;
}

.app-bootstrap-placeholder__logo {
  width: 2rem;
  height: 2rem;
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

@keyframes app-bootstrap-spin {
  to {
    transform: rotate(360deg);
  }
}
</style>
