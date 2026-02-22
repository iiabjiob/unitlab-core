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
      return "bg-green-500"
    case "offline":
      return "bg-gray-600"
    default:
      return "bg-gray-600"
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
    <SidebarListItem :active="active" class="relative" @select="handleSelect">
      <template #prefix>
        <span class="w-2 h-2 rounded-full transition-colors" :class="statusClass" />
      </template>
      <span class="truncate text-sm">
        <p class="font-semibold text-sm text-neutral-900 dark:text-neutral-100 flex items-center gap-2">
          <span>{{ device.display_name }}</span>
          <span v-if="device.name" class="text-xs text-neutral-500 dark:text-neutral-400">· {{ device.unit_id }}</span>
        </p>
      </span>
      <template #suffix>
        <span class="text-xs truncate text-neutral-500 dark:text-neutral-400">
          {{ device.device_type }}
        </span>
      </template>
    </SidebarListItem>
    </UiMenuTrigger>
    <UiMenuContent>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-100" @select="openInNewTab">
        Open in new tab
      </UiMenuItem>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-100" @select="openRename">
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
