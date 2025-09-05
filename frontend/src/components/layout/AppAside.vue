<template>
  <aside
    class="bg-white dark:bg-neutral-800 flex flex-col w-80 border-r border-neutral-200 dark:border-neutral-700"
    :class="status === 'offline' ? 'opacity-60 pointer-events-none select-none' : ''"
    :aria-disabled="status === 'offline'"
  >
    <div class="px-5 py-3 border-b border-neutral-200">
      <div class="flex justify-between items-center gap-3">
        <AppLogo/>
        <OnlineStatusComponent :status="status"/>
      </div>
      <TimeComponent class="text-sm ml-auto" />
    </div>
    <AppMenu />
    <div class="border-t border-neutral-200 dark:border-neutral-700">

      <EventLog />
    </div>
  </aside>
</template>

<script setup lang="ts">
import { computed, onMounted } from "vue"
import { useWebSocketStore } from "@/stores/websocketStore"
import { useEventLogStore } from "@/stores/eventLogStore"
import AppMenu from "./AppMenu.vue"
import EventLog from "./EventLog.vue"
import AppLogo from "../AppLogo.vue"
import TimeComponent from "../TimeComponent.vue"
import OnlineStatusComponent from "../OnlineStatusComponent.vue"

const wsStore = useWebSocketStore()
const eventLogStore = useEventLogStore()

const status = computed(() => {
  if (wsStore.isConnected) return "online"
  if (!wsStore.isConnected && wsStore.everConnected) return "offline"
  return "connecting"
})

onMounted(() => {
  if (!eventLogStore.items.length) {
    eventLogStore.fetchEvents()
  }
})
</script>
