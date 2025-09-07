<template>
  <aside
    class="bg-white dark:bg-neutral-800 flex flex-col h-full"
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

    <!-- Menu (растягивается на всё доступное место) -->
    <AppMenu class="text-base flex-1 overflow-auto" />

    <!-- Event log прижат вниз -->
    <div class="border-t border-neutral-200 dark:border-neutral-700 mt-auto" v-if="meta.globalEventLog">
      <EventLog />
    </div>
  </aside>
</template>


<script setup lang="ts">
import { computed, onMounted } from "vue"
import { useRoute } from "vue-router"
import { useWebSocketStore } from "@/stores/websocketStore"
import { useEventLogStore } from "@/stores/eventLogStore"
import AppMenu from "./AppMenu.vue"
import EventLog from "./EventLog.vue"
import AppLogo from "./AppLogo.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"

const wsStore = useWebSocketStore()
const eventLogStore = useEventLogStore()
const route = useRoute()

const status = computed(() => {
  if (wsStore.isConnected){ console.log(status); return "online"}
  if (!wsStore.isConnected && wsStore.everConnected) return "offline"
  return "offline"
})

// Meta flags (local read)
const meta = computed(() => ({
  globalEventLog: route.meta.globalEventLog ?? true,
}))

</script>
