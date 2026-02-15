<template>
  <!-- choose layout component -->
  <component :is="layoutComp" />
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onBeforeUnmount } from "vue"
import { useRoute } from "vue-router"
import { storeToRefs } from "pinia"

import DesktopLayout from "./DesktopLayout.vue"
import MobileLayout from "./MobileLayout.vue"
import { useWebSocketStore } from "@/stores/websocketStore"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import DisconnectedMobileLayout from "./DisconnectedMobileLayout.vue"
import DisconnectedDesktopLayout from "./DisconnectedDesktopLayout.vue"
import WelcomeLayout from "./WelcomeLayout.vue"

const isMobile = ref(false)
const systemHealthStore = useSystemHealthStore()

function checkMobile() {
  isMobile.value = window.innerWidth < 768
}
onMounted(() => {
  checkMobile()
  window.addEventListener("resize", checkMobile)
  void systemHealthStore.refresh()
})
onBeforeUnmount(() => {
  window.removeEventListener("resize", checkMobile)
})

const route = useRoute()
const wsStore = useWebSocketStore()
const {
  isConnected: wsIsConnected,
  everConnected: wsEverConnected,
  hasStarted: wsHasStarted,
  isConnecting: wsIsConnecting,
  reconnectAttempts: wsReconnectAttempts,
} = storeToRefs(wsStore)

// Derived connection state
const wsStatus = computed(() => {
  if (wsIsConnected.value) return "connected"
  if (!wsHasStarted.value) return "initial"
  if (wsIsConnecting.value && !wsEverConnected.value && wsReconnectAttempts.value === 0) {
    return "initial"
  }
  return "lost"
})

type LayoutMode = "auto" | "app" | "mobile" | "welcome"

// Decide layout: meta.layout = 'app' | 'mobile' | 'auto' | 'welcome'
const layoutComp = computed(() => {
  const mode = (route.meta.layout as LayoutMode | undefined) ?? "auto"

  if (mode === "welcome") {
    return WelcomeLayout
  }

  if (wsStatus.value !== "connected") {
    return isMobile.value ? DisconnectedMobileLayout : DisconnectedDesktopLayout
  }

  if (mode === "mobile") return MobileLayout
  if (mode === "app") return DesktopLayout
  return isMobile.value ? MobileLayout : DesktopLayout
})
</script>
