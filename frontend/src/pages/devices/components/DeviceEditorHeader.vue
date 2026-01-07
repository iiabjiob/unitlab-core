<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { Device } from "@/types/device"
import OnlineStatusComponent from "@/components/misc/OnlineStatusComponent.vue"
import { useDeviceStore } from "@/stores/deviceStore"
import RenameModal from "@/components/ui/RenameModal.vue"
import UiButton from "@/components/ui/UiButton.vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@affino/menu-vue"
import EllipsisHorizontalIcon from "@/components/icons/EllipsisHorizontalIcon.vue"

const props = defineProps<{
  device: Device
}>()

const deviceStore = useDeviceStore()
const renameOpen = ref(false)
const renameValue = ref(props.device.name ?? "")
const renameError = ref("")
const renaming = ref(false)

const lastSeen = computed(() => {
  const d = new Date(props.device.last_seen || Date.now())
  return d.toLocaleString("en-GB", {
    year: "numeric",
    month: "short",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
  })
})

const deviceTypeLabel = computed(() => {
  const base = props.device.type ?? (props.device as any).device_type ?? ""
  return base ? base.toUpperCase() : "UNKNOWN"
})

watch(
  () => props.device.name,
  (value) => {
    if (!renameOpen.value) {
      renameValue.value = value ?? ""
    }
  },
)

function openRename() {
  renameValue.value = props.device.name ?? ""
  renameError.value = ""
  renameOpen.value = true
}

function cancelRename() {
  if (renaming.value) return
  renameOpen.value = false
  renameError.value = ""
  renameValue.value = props.device.name ?? ""
}

async function confirmRename() {
  if (renaming.value) return
  const trimmed = renameValue.value.trim()
  const nextName = trimmed.length ? trimmed : null
  const current = props.device.name ?? null

  if (nextName === current) {
    renameOpen.value = false
    return
  }

  renameError.value = ""
  renaming.value = true
  try {
    await deviceStore.updateDeviceField(props.device.id, { name: nextName })
    renameOpen.value = false
  } catch (error) {
    renameError.value = error instanceof Error ? error.message : "Failed to rename device"
  } finally {
    renaming.value = false
  }
}
</script>

<template>
  <div class="px-4 py-3 flex items-start justify-between border-b border-neutral-300 dark:border-neutral-800">
    <!-- LEFT SIDE -->
    <div class="flex flex-col gap-1">

      <div class="text-lg font-medium tracking-tight text-neutral-900 dark:text-white flex flex-wrap items-center gap-2">
        <template v-if="device.name">
          <span>{{ device.name }}</span>
          <span class="text-neutral-400">·</span>
          <span class="text-neutral-500 dark:text-neutral-400">{{ device.unit_id }}</span>
        </template>
        <template v-else>
          {{ device.unit_id }}
        </template>
      </div>

      <div class="flex flex-wrap items-center gap-3 text-sm text-neutral-500 dark:text-neutral-400">
        <span class="text-xs uppercase tracking-[0.3em]">Device</span>
        <span>·</span>
        <span>Type {{ deviceTypeLabel }}</span>
        <template v-if="device.name">
          <span>·</span>
          <span>{{ device.name }}</span>
        </template>
        <template v-if="device.firmware_version">
          <span>·</span>
          <span>Firmware {{ device.firmware_version }}</span>
        </template>
        <span>·</span>
        <span>Last seen {{ lastSeen }}</span>
        <span>·</span>
        <OnlineStatusComponent :status="device.status" />
      </div>
    </div>

    <UiMenu>
      <UiMenuTrigger asChild>
        <UiButton variant="icon" aria-label="Device actions">
          <EllipsisHorizontalIcon size="24" />
        </UiButton>
      </UiMenuTrigger>
      <UiMenuContent>
        <UiMenuItem class="text-neutral-900 dark:text-neutral-100" @select="openRename">
          Rename
        </UiMenuItem>
      </UiMenuContent>
    </UiMenu>
  </div>

  <RenameModal
    :open="renameOpen"
    title="Rename device"
    label="Name"
    v-model="renameValue"
    :loading="renaming"
    :error="renameError"
    @cancel="cancelRename"
    @confirm="confirmRename"
  />
</template>
