<template>
  <div v-if="!appReady" class="app-bootstrap-placeholder" role="status" aria-live="polite" aria-busy="true">
    <div class="app-bootstrap-placeholder__chip">
      <span class="app-bootstrap-placeholder__spinner" aria-hidden="true"></span>
      <span>Loading…</span>
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
}

.app-bootstrap-placeholder__chip {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  font-size: 0.85rem;
  font-weight: 600;
  color: inherit;
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
