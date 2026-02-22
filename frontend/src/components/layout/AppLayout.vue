<template>
  <!-- choose layout component -->
  <component :is="layoutComp" />
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onBeforeUnmount } from "vue"
import { useRoute } from "vue-router"

import DesktopLayout from "./DesktopLayout.vue"
import MobileLayout from "./MobileLayout.vue"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import WelcomeLayout from "./WelcomeLayout.vue"

const isMobile = ref(false)
const systemHealthStore = useSystemHealthStore()
let healthRefreshTimer: ReturnType<typeof setInterval> | null = null

function refreshSystemHealth() {
  void systemHealthStore.refresh()
}

function handleVisibilityChange() {
  if (document.visibilityState === "visible") {
    refreshSystemHealth()
  }
}

function checkMobile() {
  isMobile.value = window.innerWidth < 768
}
onMounted(() => {
  checkMobile()
  window.addEventListener("resize", checkMobile)
  refreshSystemHealth()
  document.addEventListener("visibilitychange", handleVisibilityChange)
  healthRefreshTimer = setInterval(refreshSystemHealth, 15000)
})
onBeforeUnmount(() => {
  window.removeEventListener("resize", checkMobile)
  document.removeEventListener("visibilitychange", handleVisibilityChange)
  if (healthRefreshTimer) {
    clearInterval(healthRefreshTimer)
    healthRefreshTimer = null
  }
})

const route = useRoute()

type LayoutMode = "auto" | "app" | "mobile" | "welcome"

// Decide layout: meta.layout = 'app' | 'mobile' | 'auto' | 'welcome'
const layoutComp = computed(() => {
  const mode = (route.meta.layout as LayoutMode | undefined) ?? "auto"

  if (mode === "welcome") {
    return WelcomeLayout
  }

  if (mode === "mobile") return MobileLayout
  if (mode === "app") return DesktopLayout
  return isMobile.value ? MobileLayout : DesktopLayout
})
</script>
