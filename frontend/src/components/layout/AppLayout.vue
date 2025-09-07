<template>
  <!-- choose layout component -->
  <component :is="layoutComp" />
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onBeforeUnmount } from "vue"
import { useRoute } from "vue-router"

import DesktopLayout from "./DesktopLayout.vue"
import MobileLayout from "./MobileLayout.vue"
import { useWebSocketStore } from "@/stores/websocketStore"
import DisconnectedMobileLayout from "./DisconnectedMobileLayout.vue"
import DisconnectedDesktopLayout from "./DisconnectedDesktopLayout.vue"

const isMobile = ref(false)

function checkMobile() {
  isMobile.value = window.innerWidth < 768
}
onMounted(() => {
  checkMobile()
  window.addEventListener("resize", checkMobile)
})
onBeforeUnmount(() => window.removeEventListener("resize", checkMobile))

const route = useRoute()
const wsStore = useWebSocketStore()

// Derived connection state
const wsStatus = computed(() => {
  if (!wsStore.isConnected && !wsStore.everConnected) return "initial"
  if (wsStore.isConnected) return "connected"
  return "lost"
})

// Decide layout: meta.layout = 'app' | 'mobile' | 'auto'
const layoutComp = computed(() => {
  if (wsStatus.value !== "connected") {
    return isMobile.value ? DisconnectedMobileLayout : DisconnectedDesktopLayout
  }

  const mode = (route.meta.layout as "auto" | "app" | "mobile") ?? "auto"
  if (mode === "mobile") return MobileLayout
  if (mode === "app") return DesktopLayout
  return isMobile.value ? MobileLayout : DesktopLayout
})
</script>
