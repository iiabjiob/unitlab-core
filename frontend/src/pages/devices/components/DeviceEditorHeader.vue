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
} from "@/components/ui/menu"
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
  <div class="device-editor-header">
    <div class="device-editor-header__main">
      <div class="device-editor-header__title">
        <template v-if="device.name">
          <span>{{ device.name }}</span>
          <span class="device-editor-header__separator">·</span>
          <span class="device-editor-header__unit-id">{{ device.unit_id }}</span>
        </template>
        <template v-else>
          {{ device.unit_id }}
        </template>
      </div>

      <div class="device-editor-header__meta">
        <span class="device-editor-header__eyebrow">Device</span>
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
        <UiMenuItem class="device-editor-header__menu-item" @select="openRename">
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

<style scoped>
.device-editor-header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  padding: 0.75rem 1rem;
  border-bottom: 1px solid var(--color-neutral-300);
}

.device-editor-header__main {
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
}

.device-editor-header__title {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.5rem;
  color: var(--color-neutral-900);
  font-size: var(--text-lg);
  font-weight: 500;
  letter-spacing: 0;
}

.device-editor-header__separator {
  color: var(--color-neutral-400);
}

.device-editor-header__unit-id {
  color: var(--color-neutral-500);
}

.device-editor-header__meta {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.75rem;
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

.device-editor-header__eyebrow {
  font-size: var(--text-xs);
  letter-spacing: 0;
  text-transform: uppercase;
}

.device-editor-header__menu-item {
  color: var(--color-neutral-900);
}

:global(.dark .device-editor-header) {
  border-bottom-color: var(--color-neutral-800);
}

:global(.dark .device-editor-header__title) {
  color: var(--color-white);
}

:global(.dark .device-editor-header__unit-id),
:global(.dark .device-editor-header__meta) {
  color: var(--color-neutral-400);
}

:global(.dark .device-editor-header__menu-item) {
  color: var(--color-neutral-100);
}
</style>
