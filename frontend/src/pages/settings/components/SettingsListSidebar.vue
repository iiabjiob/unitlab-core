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
const releaseVersion = import.meta.env.VITE_RELEASE_VERSION ?? "dev"

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
  <div class="settings-list-sidebar">
    <div class="settings-list-sidebar__header">
      <p class="settings-list-sidebar__eyebrow">Settings</p>
      <h2 class="settings-list-sidebar__title">Core Services</h2>
      <p class="settings-list-sidebar__description">
        Host-level services for the central module (diagnostics, time sync, and future infrastructure tools).
      </p>
    </div>

    <div class="settings-list-sidebar__list">
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

    <p class="settings-list-sidebar__build">Build {{ releaseVersion }}</p>
  </div>
</template>

<style scoped>
.settings-list-sidebar {
  display: flex;
  height: 100%;
  flex-direction: column;
}

.settings-list-sidebar__header {
  margin-bottom: 0.75rem;
}

.settings-list-sidebar__eyebrow {
  color: var(--color-neutral-500);
  font-size: 11px;
  letter-spacing: 0;
  text-transform: uppercase;
}

.settings-list-sidebar__title {
  margin-top: 0.25rem;
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 600;
}

.settings-list-sidebar__description {
  margin-top: 0.25rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.settings-list-sidebar__list {
  flex: 1 1 0%;
  overflow-y: auto;
}

.settings-list-sidebar__build {
  margin-top: 0.75rem;
  color: var(--color-neutral-500);
  font-size: 11px;
}

:global(.dark .settings-list-sidebar__eyebrow),
:global(.dark .settings-list-sidebar__description),
:global(.dark .settings-list-sidebar__build){
  color: var(--color-neutral-400);
}

:global(.dark .settings-list-sidebar__title){
  color: var(--color-neutral-100);
}
</style>
