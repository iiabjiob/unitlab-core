<script setup lang="ts">
import SidebarListItem from "@/components/ui/SidebarListItem.vue"
import type { Device } from "@/types/device"
import { computed, ref, watch } from "vue"
import { useRouter } from "vue-router"
import RenameModal from "@/components/ui/RenameModal.vue"
import { useDeviceStore } from "@/stores/deviceStore"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@/components/ui/menu"

const props = defineProps<{
  device: Device
  active: boolean
}>()

const emit = defineEmits<{ (e: "select", id: number): void }>()
const deviceStore = useDeviceStore()
const router = useRouter()
const renameOpen = ref(false)
const renameValue = ref(props.device.name ?? "")
const renaming = ref(false)
const renameError = ref("")

const statusClass = computed(() => {
  switch (props.device.status) {
    case "online":
      return "device-list-item__status--online"
    case "offline":
      return "device-list-item__status--offline"
    default:
      return "device-list-item__status--offline"
  }
})

function handleSelect() {
  emit("select", props.device.id)
}

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

function openInNewTab() {
  const resolved = router.resolve({ name: "devices.detail", params: { id: props.device.id } })
  if (typeof window !== "undefined") {
    window.open(resolved.href, "_blank", "noopener,noreferrer")
  }
}
</script>

<template>
  <UiMenu>
    <UiMenuTrigger as-child trigger="contextmenu">
      <SidebarListItem :active="active" class="device-list-item" @select="handleSelect">
        <template #prefix>
          <span class="device-list-item__status" :class="statusClass" />
        </template>
        <span class="device-list-item__body">
          <span class="device-list-item__title-row">
            <span>{{ device.display_name }}</span>
            <span v-if="device.name" class="device-list-item__unit-id">· {{ device.unit_id }}</span>
          </span>
        </span>
        <template #suffix>
          <span class="device-list-item__type">
            {{ device.device_type }}
          </span>
        </template>
      </SidebarListItem>
    </UiMenuTrigger>
    <UiMenuContent>
      <UiMenuItem class="device-list-item__menu-item" @select="openInNewTab">
        Open in new tab
      </UiMenuItem>
      <UiMenuItem class="device-list-item__menu-item" @select="openRename">
        Rename
      </UiMenuItem>
    </UiMenuContent>
  </UiMenu>

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
.device-list-item {
  position: relative;
}

.device-list-item__status {
  width: 0.5rem;
  height: 0.5rem;
  border-radius: 9999px;
  transition: background-color 120ms ease;
}

.device-list-item__status--online {
  background: var(--color-green-600);
}

.device-list-item__status--offline {
  background: var(--color-neutral-600);
}

.device-list-item__body {
  overflow: hidden;
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  text-overflow: ellipsis;
  white-space: nowrap;
}

.device-list-item__title-row {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  font-size: var(--text-sm);
  font-weight: 600;
}

.device-list-item__unit-id {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  font-weight: 400;
}

.device-list-item__type {
  overflow: hidden;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  text-overflow: ellipsis;
  white-space: nowrap;
}

.device-list-item__menu-item {
  color: var(--color-neutral-900);
}

:global(.dark .device-list-item__body),
:global(.dark .device-list-item__menu-item) {
  color: var(--color-neutral-100);
}

:global(.dark .device-list-item__unit-id),
:global(.dark .device-list-item__type) {
  color: var(--color-neutral-400);
}
</style>
