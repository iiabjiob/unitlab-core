<template>
  <component :is="isMobile ? MobileLayout : DesktopLayout" />
</template>

<script setup lang="ts">
import { ref, onMounted, onBeforeUnmount } from "vue"
import DesktopLayout from "./DesktopLayout.vue"
import MobileLayout from "./MobileLayout.vue"

const isMobile = ref(false)

function checkMobile() {
  isMobile.value = window.innerWidth < 768 // md breakpoint
}

onMounted(() => {
  checkMobile()
  window.addEventListener("resize", checkMobile)
})
onBeforeUnmount(() => window.removeEventListener("resize", checkMobile))
</script>
