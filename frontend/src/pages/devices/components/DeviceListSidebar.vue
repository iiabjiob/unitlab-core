<script setup lang="ts">
import { ref, computed } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useRouter, useRoute } from "vue-router"
import DeviceListItem from "./DeviceListItem.vue"
import UiSidebarListbox from "@/components/ui/UiSidebarListbox.vue"
import UiButton from "@/components/ui/UiButton.vue"
import RenameModal from "@/components/ui/RenameModal.vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@affino/menu-vue"
import EllipsisHorizontalIcon from "@/components/icons/EllipsisHorizontalIcon.vue"

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

const filteredDevices = computed(() => {
  if (!query.value.trim()) return store.devices

  const q = query.value.toLowerCase()

  return store.devices.filter(s =>
    s.unit_id.toLowerCase().includes(q) ||
    s.display_name.toLowerCase().includes(q) ||
    (s.name && s.name.toLowerCase().includes(q))
  )
})

const selectedId = computed<number | null>(() => {
  const parsed = Number(route.params.id)
  return Number.isFinite(parsed) ? parsed : null
})

const selectedDevice = computed(() => {
  if (selectedId.value === null) return null
  return store.devices.find((device) => device.id === selectedId.value) ?? null
})

const renameOpen = ref(false)
const renameValue = ref("")
const renaming = ref(false)
const renameError = ref("")

function handleSelect(id: string | number) {
  const parsed = Number(id)
  if (!Number.isFinite(parsed)) return
  openDevice(parsed)
}

function openRenameSelected() {
  if (!selectedDevice.value) return
  renameValue.value = selectedDevice.value.name ?? ""
  renameError.value = ""
  renameOpen.value = true
}

function cancelRenameSelected() {
  if (renaming.value) return
  renameOpen.value = false
  renameError.value = ""
  renameValue.value = selectedDevice.value?.name ?? ""
}

async function confirmRenameSelected() {
  if (!selectedDevice.value || renaming.value) return

  const trimmed = renameValue.value.trim()
  const nextName = trimmed.length ? trimmed : null
  const current = selectedDevice.value.name ?? null
  if (nextName === current) {
    renameOpen.value = false
    return
  }

  renameError.value = ""
  renaming.value = true
  try {
    await store.updateDeviceField(selectedDevice.value.id, { name: nextName })
    renameOpen.value = false
  } catch (error) {
    renameError.value = error instanceof Error ? error.message : "Failed to rename device"
  } finally {
    renaming.value = false
  }
}
</script>

<template>
  <div class="h-full flex flex-col">

    <div class="mb-3 flex items-center justify-end">
      <UiMenu v-if="selectedDevice">
        <UiMenuTrigger asChild>
          <UiButton
            variant="icon"
            aria-label="Device actions"
            @click.stop
            @pointerdown.stop
          >
            <EllipsisHorizontalIcon size="20" />
          </UiButton>
        </UiMenuTrigger>
        <UiMenuContent>
          <UiMenuItem class="text-neutral-900 dark:text-neutral-100" @select="openRenameSelected">
            Rename
          </UiMenuItem>
        </UiMenuContent>
      </UiMenu>
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

    <RenameModal
      :open="renameOpen"
      title="Rename device"
      label="Name"
      v-model="renameValue"
      :loading="renaming"
      :error="renameError"
      @cancel="cancelRenameSelected"
      @confirm="confirmRenameSelected"
    />

  </div>
</template>
