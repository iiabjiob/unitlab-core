<template>
  <!-- choose layout component -->
  <component :is="layoutComp" />
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useRoute } from "vue-router"

import DesktopLayout from "./DesktopLayout.vue"
import MobileLayout from "./MobileLayout.vue"
import WelcomeLayout from "./WelcomeLayout.vue"
import { useViewport } from "@/composables/useViewport"

const { isMobile } = useViewport()

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
