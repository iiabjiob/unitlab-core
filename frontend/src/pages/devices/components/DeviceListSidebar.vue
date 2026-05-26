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
  <div class="h-full flex flex-col">

    <div class="mb-2">
      <label class="inline-flex items-center gap-2 text-xs text-neutral-700 dark:text-neutral-300 cursor-default select-none">
        <input
          v-model="onlineOnly"
          type="checkbox"
          autocomplete="off"
          name="device-online-only"
          class="h-4 w-4 rounded border-neutral-300 text-primary-600 focus:ring-primary-500"
        />
        <span>Online only</span>
      </label>
    </div>

    <!-- SEARCH FIELD -->
    <div class="mb-3">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="device-search"
        placeholder="Search devices…"
        class="w-full rounded-lg border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
      />
    </div>

    <!-- LIST -->
    <UiSidebarListbox
      class="flex-1"
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
        <div class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
          No devices found
        </div>
      </template>
    </UiSidebarListbox>

  </div>
</template>
