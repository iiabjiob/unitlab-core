<script setup lang="ts">
import { computed, watch } from "vue"
import { useRoute, useRouter } from "vue-router"
import UiSidebarListbox from "@/components/ui/UiSidebarListbox.vue"
import { SETTINGS_SERVICE_MODE_ENABLED } from "@/config/settingsFeatures"
import { useSelectionStore } from "@/stores/selectionStore"
import SettingsListItem from "./SettingsListItem.vue"

type SettingsNavItem = {
  id: string
  label: string
  description: string
  routeName: string
}

const route = useRoute()
const router = useRouter()
const selectionStore = useSelectionStore()
selectionStore.restore()

const baseItems: SettingsNavItem[] = [
  {
    id: "diagnostics",
    label: "Core Diagnostics",
    description: "RPi host health, thermals, load, disk and services",
    routeName: "settings.diagnostics",
  },
  {
    id: "ntp",
    label: "Time / NTP Sync",
    description: "Chrony servers and sync health",
    routeName: "settings.ntp",
  },
  {
    id: "network",
    label: "Core Network",
    description: "AP / STA uplink and access point lifecycle",
    routeName: "settings.network",
  },
  {
    id: "updates",
    label: "Updates",
    description: "Core software updates (planned)",
    routeName: "settings.updates",
  },
  {
    id: "provisioning",
    label: "Provisioning",
    description: "Golden flash and post-flash actions (planned)",
    routeName: "settings.provisioning",
  },
]

const items = computed<SettingsNavItem[]>(() => {
  if (SETTINGS_SERVICE_MODE_ENABLED) {
    return baseItems
  }
  return baseItems.filter(item => item.id !== "updates" && item.id !== "provisioning")
})

const selectedId = computed<string | null>(() => {
  const name = String(route.name ?? "")
  if (SETTINGS_SERVICE_MODE_ENABLED && name === "settings.provisioning") return "provisioning"
  if (SETTINGS_SERVICE_MODE_ENABLED && name === "settings.updates") return "updates"
  if (name === "settings.ntp") return "ntp"
  if (name === "settings.diagnostics") return "diagnostics"
  if (name === "settings.network") return "network"
  return "diagnostics"
})

watch(
  () => route.name,
  (name) => {
    const routeName = name ? String(name) : null
    if (!routeName?.startsWith("settings.")) return
    selectionStore.selectSettingsRoute(routeName)
  },
  { immediate: true },
)

function handleSelect(id: string | number) {
  const target = items.value.find(item => item.id === String(id))
  if (!target) return
  void router.push({ name: target.routeName })
}
</script>

<template>
  <div class="h-full flex flex-col">
    <div class="mb-3">
      <p class="text-[11px] uppercase tracking-[0.26em] text-neutral-500 dark:text-neutral-400">Settings</p>
      <h2 class="mt-1 text-sm font-semibold text-neutral-900 dark:text-neutral-100">Core Services</h2>
      <p class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">
        Host-level services for the central module (network, time sync, and future infrastructure tools).
      </p>
    </div>

    <div class="flex-1 overflow-y-auto">
      <UiSidebarListbox
        :items="items"
        :active-id="selectedId"
        aria-label="Settings sections"
        @select="handleSelect"
      >
        <template #item="{ item, isCursor }">
          <SettingsListItem
            :label="item.label"
            :description="item.description"
            :active="selectedId === item.id || isCursor"
          />
        </template>
      </UiSidebarListbox>
    </div>
  </div>
</template>
