<template>
  <aside
    class="flex flex-col h-full relative"
    :class="status === 'offline' ? 'opacity-60 pointer-events-none select-none' : ''"
    :aria-disabled="status === 'offline'"
  >
    <!-- Header -->
    <div class="px-5 border-b border-neutral-200 dark:border-neutral-700 h-20 flex flex-col justify-center gap-2">
      <div class="flex justify-between items-center gap-3">
        <AppLogo />
        <OnlineStatusComponent :status="status" :description="statusDescription" />
      </div>
      <TimeComponent class="text-sm" />
    </div>

    <!-- Menu stretches to fill available space while leaving room for the footer -->
    <AppMenu class="text-base overflow-auto"/>

    <div class="p-5 mx-auto"><ThemeToggle /></div>

  </aside>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useWebSocketStore } from "@/stores/websocketStore"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import AppMenu from "./AppMenu.vue"
import AppLogo from "./AppLogo.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import ThemeToggle from "../ui/ThemeToggle.vue"

const wsStore = useWebSocketStore()
const systemHealthStore = useSystemHealthStore()

const status = computed(() => {
  if (!wsStore.isConnected) return "offline"
  return systemHealthStore.status
})

const statusDescription = computed(() => {
  if (!wsStore.isConnected) return "No connection to the server"
  return systemHealthStore.tooltip
})

</script>
