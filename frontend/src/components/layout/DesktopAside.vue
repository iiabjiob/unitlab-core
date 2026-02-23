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
    <AppMenu :compact="compact" :include-settings="false" class="text-base overflow-y-auto overflow-x-visible"/>

    <div class="border-t border-neutral-200 dark:border-neutral-700" :class="compact ? 'p-2' : 'p-4'">
      <div class="flex items-center justify-center gap-2" :class="compact ? 'flex-col' : ''">
        <RouterLink
          to="/settings"
          class="inline-flex items-center justify-center rounded-lg border border-neutral-300 text-neutral-700 transition hover:bg-neutral-100 dark:border-neutral-700 dark:text-neutral-200 dark:hover:bg-neutral-800"
          :class="compact ? 'h-10 w-10' : 'h-9 px-3 gap-2'"
          title="Settings"
          aria-label="Settings"
        >
          <SystemIcon class="h-4 w-4" />
          <span v-if="!compact" class="text-xs font-semibold">Settings</span>
        </RouterLink>
        <ThemeToggle v-if="!compact" />
      </div>
    </div>

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
import SystemIcon from "@/components/icons/SystemIcon.vue"

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
