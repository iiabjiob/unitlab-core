<template>
  <aside
    class="bg-white dark:bg-neutral-800 flex flex-col w-80 border-r border-neutral-200 dark:border-neutral-700"
    :class="wsStatus==='offline' ? 'opacity-60 pointer-events-none select-none' : ''"
    :aria-disabled="wsStatus==='offline'"
    >

    <div class="flex flex-col px-5 py-3 border-b border-neutral-200 dark:border-neutral-700">

      <div class="flex items-center justify-between gap-3">
        <AppLogo/>
        <OnlineStatusComponent :status="wsStatus"/>
      </div>
      <TimeComponent class="text-sm"/>
    </div>

    <!-- Menu -->
    <nav class="flex-1 py-2 space-y-1 px-2">
      <!-- Devices -->
      <RouterLink
        to="/devices"
        class="flex items-center rounded h-8 px-2 transition-colors text-neutral-600 dark:text-neutral-300 hover:bg-neutral-100 dark:hover:bg-neutral-700"
        active-class="bg-neutral-100 dark:bg-neutral-700 font-semibold text-neutral-900 dark:text-white"
      >
        <DraftIcon class="w-16" size="20" />
        <span>Devices</span>
      </RouterLink>

      <!-- Switchgear -->
      <RouterLink
        to="/switchgear"
        class="flex items-center rounded h-8 px-2 transition-colors text-neutral-600 dark:text-neutral-300 hover:bg-neutral-100 dark:hover:bg-neutral-700"
        active-class="bg-neutral-100 dark:bg-neutral-700 font-semibold text-neutral-900 dark:text-white"
      >
        <DraftIcon class="w-16" size="20" />
        <span>Switchgear</span>
      </RouterLink>

      <!-- SignalList -->
      <RouterLink
        to="/signal-list"
        class="flex items-center rounded h-8 px-2 transition-colors text-neutral-600 dark:text-neutral-300 hover:bg-neutral-100 dark:hover:bg-neutral-700"
        active-class="bg-neutral-100 dark:bg-neutral-700 font-semibold text-neutral-900 dark:text-white"
      >
        <DraftIcon class="w-16" size="20" />
        <span>Signal List</span>
      </RouterLink>

      <!-- Sequences -->
      <RouterLink
        to="/sequences"
        class="flex items-center rounded h-8 px-2 transition-colors text-neutral-600 dark:text-neutral-300 hover:bg-neutral-100 dark:hover:bg-neutral-700"
        active-class="bg-neutral-100 dark:bg-neutral-700 font-semibold text-neutral-900 dark:text-white"
      >
        <DraftIcon class="w-16" size="20" />
        <span>Sequences</span>
      </RouterLink>

      <!-- Events -->
      <RouterLink
        to="/events"
        class="flex items-center rounded h-8 px-2 transition-colors text-neutral-600 dark:text-neutral-300 hover:bg-neutral-100 dark:hover:bg-neutral-700"
        active-class="bg-neutral-100 dark:bg-neutral-700 font-semibold text-neutral-900 dark:text-white"
      >
        <DraftIcon class="w-16" size="20" />
        <span>Events</span>
      </RouterLink>
    </nav>

    <!-- Event Log (footer) -->
    <AsideEventLogComponent/>

  </aside>
</template>

<script setup lang="ts">
import DraftIcon from "./icons/DraftIcon.vue";
import AppLogo from "./AppLogo.vue";

import { onMounted, computed } from "vue"
import { useEventLogStore } from "@/stores/eventLogStore";
import { useWebSocketStore } from "@/stores/websocketStore";
import AsideEventLogComponent from "./AsideEventLogComponent.vue";
import TimeComponent from "./TimeComponent.vue";
import OnlineStatusComponent from "./OnlineStatusComponent.vue";

const eventLogStore = useEventLogStore();
const wsStore =useWebSocketStore()

const wsStatus = computed(() => {
  if (wsStore.isConnected) return "online"
  if (!wsStore.isConnected && wsStore.everConnected) return "offline"
  return "connecting"
})

onMounted(() => {
  if (!eventLogStore.items.length) {
    eventLogStore.fetchEvents() // load events
  }
})
</script>
