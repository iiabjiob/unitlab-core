<script setup lang="ts">
import { ref, computed, watch } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useRouter, useRoute } from "vue-router"
import DeviceListItem from "./DeviceListItem.vue"
import UiSidebarListbox from "@/components/ui/UiSidebarListbox.vue"
import { localSettingsKeys, readBooleanLocalSetting, writeLocalSetting } from "@/services/localSettingsStorage"

const store = useDeviceStore()
const router = useRouter()
const route = useRoute()

function isActive(id: number) {
  return Number(route.params.id) === id
}

function openDevice(id: number) {
  if (Number(route.params.id) === id) {
    return
  }
  void router.push(`/devices/${id}`)
}


// SEARCH
const query = ref("")
const onlineOnly = ref(false)
const LEGACY_ONLINE_ONLY_STORAGE_KEY = "unitlab.devices.sidebar.online-only"

function restoreOnlineOnlyFilter() {
  onlineOnly.value = readBooleanLocalSetting(localSettingsKeys.devicesSidebarOnlineOnly, false, {
    legacyKeys: [LEGACY_ONLINE_ONLY_STORAGE_KEY],
    parseLegacy: parseLegacyBooleanFlag,
  })
}

function persistOnlineOnlyFilter(value: boolean) {
  writeLocalSetting(localSettingsKeys.devicesSidebarOnlineOnly, value, {
    legacyKeys: [LEGACY_ONLINE_ONLY_STORAGE_KEY],
  })
}

restoreOnlineOnlyFilter()

watch(onlineOnly, (value) => {
  persistOnlineOnlyFilter(value)
})

const filteredDevices = computed(() => {
  const source = onlineOnly.value
    ? store.devices.filter(device => device.online)
    : store.devices

  if (!query.value.trim()) return source

  const q = query.value.toLowerCase()

  return source.filter(s =>
    s.unit_id.toLowerCase().includes(q) ||
    s.display_name.toLowerCase().includes(q) ||
    (s.name && s.name.toLowerCase().includes(q))
  )
})

const selectedId = computed<number | null>(() => {
  const parsed = Number(route.params.id)
  return Number.isFinite(parsed) ? parsed : null
})

function handleSelect(id: string | number) {
  const parsed = Number(id)
  if (!Number.isFinite(parsed)) return
  openDevice(parsed)
}

function parseLegacyBooleanFlag(raw: string): boolean {
  return raw === "1" || raw === "true"
}
</script>

<template>
  <div class="device-list-sidebar">

    <div class="device-list-sidebar__filter">
      <label class="device-list-sidebar__checkbox-label">
        <input
          v-model="onlineOnly"
          type="checkbox"
          autocomplete="off"
          name="device-online-only"
          class="device-list-sidebar__checkbox"
        />
        <span>Online only</span>
      </label>
    </div>

    <!-- SEARCH FIELD -->
    <div class="device-list-sidebar__search">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="device-search"
        placeholder="Search devices…"
        class="device-list-sidebar__input"
      />
    </div>

    <!-- LIST -->
    <UiSidebarListbox
      class="device-list-sidebar__list"
      :items="filteredDevices"
      :active-id="selectedId"
      aria-label="Devices"
      @select="handleSelect"
    >
      <template #item="{ item: device, isCursor }">
        <DeviceListItem
          :device="device"
          :active="isActive(device.id) || isCursor"
        />
      </template>
      <template #empty>
        <div class="device-list-sidebar__empty">
          No devices found
        </div>
      </template>
    </UiSidebarListbox>

  </div>
</template>

<style scoped>
.device-list-sidebar {
  display: flex;
  height: 100%;
  flex-direction: column;
}

.device-list-sidebar__filter {
  margin-bottom: 0.5rem;
}

.device-list-sidebar__checkbox-label {
  display: inline-flex;
  user-select: none;
  align-items: center;
  gap: 0.5rem;
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
}

.device-list-sidebar__checkbox {
  width: 1rem;
  height: 1rem;
  border-radius: var(--radius-sm);
  accent-color: var(--color-blue-600);
}

.device-list-sidebar__search {
  margin-bottom: 0.75rem;
}

.device-list-sidebar__input {
  width: 100%;
  padding: 0.5rem 0.75rem;
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-md);
  background: var(--color-white);
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  outline: none;
}

.device-list-sidebar__input::placeholder {
  color: var(--color-neutral-500);
}

.device-list-sidebar__input:focus {
  border-color: var(--color-neutral-500);
}

.device-list-sidebar__list {
  flex: 1 1 0%;
}

.device-list-sidebar__empty {
  padding: 1.5rem 1rem;
  border: 1px dashed color-mix(in srgb, var(--color-neutral-300) 70%, transparent);
  border-radius: 1rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  text-align: center;
}

:global(.dark .device-list-sidebar__checkbox-label) {
  color: var(--color-neutral-300);
}

:global(.dark .device-list-sidebar__input) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-950);
  color: var(--color-neutral-100);
}

:global(.dark .device-list-sidebar__empty) {
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-400);
}
</style>
