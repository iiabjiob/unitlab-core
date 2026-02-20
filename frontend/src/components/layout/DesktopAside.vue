<template>
  <aside class="flex flex-col h-full relative">
    <!-- Header -->
    <div
      class="border-b border-neutral-200 dark:border-neutral-700 h-20 flex flex-col justify-center gap-2"
      :class="compact ? 'px-2 items-center' : 'px-5'"
    >
      <div class="flex justify-between items-center gap-3" :class="compact ? 'w-full justify-center' : ''">
        <RouterLink
          v-if="compact"
          to="/"
          class="inline-flex h-9 w-9 items-center justify-center rounded-lg border border-neutral-300 text-xs font-semibold text-neutral-700 hover:bg-neutral-100 dark:border-neutral-700 dark:text-neutral-200 dark:hover:bg-neutral-800"
          title="Home"
          aria-label="Home"
        >
          UL
        </RouterLink>
        <span
          v-if="compact"
          class="h-2.5 w-2.5 rounded-full border border-white/70 shadow-sm"
          :class="compactStatusClass"
          :title="`System status: ${status}`"
          aria-hidden="true"
        />
        <AppLogo v-else />
        <OnlineStatusComponent
          v-if="!compact"
          :status="status"
          :description="statusDescription"
          neutral-offline
        />
      </div>
      <TimeComponent
        v-if="!compact"
        class="text-xs text-neutral-500 dark:text-neutral-400"
      />
    </div>

    <!-- Menu stretches to fill available space while leaving room for the footer -->
    <AppMenu :compact="compact" class="text-base overflow-y-auto overflow-x-visible"/>

    <div v-if="!compact" class="p-5 mx-auto"><ThemeToggle /></div>

  </aside>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { RouterLink } from "vue-router"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import AppMenu from "./AppMenu.vue"
import AppLogo from "./AppLogo.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import ThemeToggle from "../ui/ThemeToggle.vue"

defineProps<{
  compact?: boolean
}>()

const systemHealthStore = useSystemHealthStore()

const status = computed(() => systemHealthStore.status)
const statusDescription = computed(() => systemHealthStore.tooltip)
const compactStatusClass = computed(() => {
  if (status.value === "online") return "bg-green-400"
  if (status.value === "degraded") return "bg-amber-400"
  if (status.value === "offline") return "bg-neutral-400"
  return "bg-neutral-400"
})

</script>
