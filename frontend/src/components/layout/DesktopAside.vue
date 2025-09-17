<template>
  <aside
    class="flex flex-col h-full relative"
    :class="status === 'offline' ? 'opacity-60 pointer-events-none select-none' : ''"
    :aria-disabled="status === 'offline'"
  >
    <!-- Header -->
    <div class="px-5 py-3 border-b border-neutral-200 dark:border-neutral-700">
      <div class="flex justify-between items-center gap-3">
        <AppLogo />
        <OnlineStatusComponent :status="status" />
      </div>
      <TimeComponent class="text-sm ml-auto" />
    </div>

    <!-- Menu (растягивается на всё доступное место, но учитывает высоту лога) -->
    <AppMenu class="text-base overflow-auto"/>

    <div class="p-5 mx-auto"><ThemeToggle /></div>

  </aside>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useWebSocketStore } from "@/stores/websocketStore"
import AppMenu from "./AppMenu.vue"
import AppLogo from "./AppLogo.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import ThemeToggle from "../ui/ThemeToggle.vue"

const wsStore = useWebSocketStore()

const status = computed(() => {
  if (wsStore.isConnected) return "online"
  if (!wsStore.isConnected && wsStore.everConnected) return "offline"
  return "offline"
})

</script>
