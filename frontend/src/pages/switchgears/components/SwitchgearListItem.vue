<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"
import SidebarListItem from "@/components/ui/SidebarListItem.vue"
import RenameModal from "@/components/ui/RenameModal.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import type { Switchgear } from "@/types/switchgear"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import SwitchgearPositionIcon from "./SwitchgearPositionIcon.vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@/components/ui/menu"

const props = defineProps<{
  switchgear: Switchgear
  active: boolean
}>()

const switchgearStore = useSwitchgearStore()
const positionState = computed(() => switchgearStore.resolveSwitchgearState(props.switchgear))
const router = useRouter()
const route = useRoute()

const emit = defineEmits<{ (e: "select", id: number): void }>()

const renameOpen = ref(false)
const renameValue = ref(props.switchgear.name)
const renaming = ref(false)
const deleteOpen = ref(false)

watch(
  () => props.switchgear.name,
  (value) => {
    if (!renameOpen.value) {
      renameValue.value = value
    }
  },
)

const deleteMessage = computed(() => `Switchgear "${props.switchgear.name}" will be deleted.`)

function handleSelect() {
  emit("select", props.switchgear.id)
}

function openRename() {
  renameValue.value = props.switchgear.name
  renameOpen.value = true
}

function cancelRename() {
  renameOpen.value = false
  renameValue.value = props.switchgear.name
}

async function confirmRename() {
  const trimmed = renameValue.value.trim()
  if (!trimmed || trimmed === props.switchgear.name) {
    renameOpen.value = false
    return
  }
  renaming.value = true
  try {
    await switchgearStore.updateField(props.switchgear.id, { name: trimmed })
    renameOpen.value = false
  } finally {
    renaming.value = false
  }
}

async function duplicateSwitchgear() {
  const duplicated = await switchgearStore.duplicate(props.switchgear.id)
  await router.push({ name: "switchgears.detail", params: { id: duplicated.id } })
}

async function confirmDelete() {
  await switchgearStore.remove(props.switchgear.id)
  deleteOpen.value = false
  if (Number(route.params.id) === props.switchgear.id) {
    await router.push({ name: "switchgears.list" })
  }
}

function openInNewTab() {
  const resolved = router.resolve({ name: "switchgears.detail", params: { id: props.switchgear.id } })
  if (typeof window !== "undefined") {
    window.open(resolved.href, "_blank", "noopener,noreferrer")
  }
}
</script>

<template>
  <UiMenu>
    <UiMenuTrigger as-child trigger="contextmenu">
    <SidebarListItem :active="active" class="relative" @select="handleSelect">
      <span class="truncate text-sm font-medium">
        {{ switchgear.name }}
      </span>
      <template #suffix>
        <span class="absolute right-3 top-1/2 -translate-y-1/2">
          <SwitchgearPositionIcon :state="positionState" />
        </span>
      </template>
      <template #subtitle>
        {{ switchgear.switchgear_type }}
      </template>
    </SidebarListItem>
    </UiMenuTrigger>
    <UiMenuContent>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="openInNewTab">
        Open in new tab
      </UiMenuItem>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="openRename">
        Rename
      </UiMenuItem>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="duplicateSwitchgear">
        Duplicate
      </UiMenuItem>
      <UiMenuItem danger @select="deleteOpen = true">
        Delete
      </UiMenuItem>
    </UiMenuContent>
  </UiMenu>

  <RenameModal
    :open="renameOpen"
    title="Rename switchgear"
    v-model="renameValue"
    :loading="renaming"
    @cancel="cancelRename"
    @confirm="confirmRename"
  />

  <ConfirmModal
    :open="deleteOpen"
    title="Delete switchgear"
    :message="deleteMessage"
    confirm-label="Delete"
    cancel-label="Cancel"
    @cancel="deleteOpen = false"
    @confirm="confirmDelete"
  />
</template>
