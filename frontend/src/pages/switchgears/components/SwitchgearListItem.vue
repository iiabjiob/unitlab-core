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
  selected?: boolean
  selectedCount?: number
}>()

const switchgearStore = useSwitchgearStore()
const positionState = computed(() => switchgearStore.resolveSwitchgearState(props.switchgear))
const router = useRouter()
const route = useRoute()

const emit = defineEmits<{
  (e: "select", id: number): void
  (e: "contextSelect", id: number): void
  (e: "deleteSelected"): void
}>()

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
const selectedCount = computed(() => Math.max(0, props.selectedCount ?? 0))
const usesSelectedDelete = computed(() => Boolean(props.selected) && selectedCount.value > 0)
const hasBulkSelection = computed(() => usesSelectedDelete.value && selectedCount.value > 1)
const deleteMenuLabel = computed(() => (
  hasBulkSelection.value ? `Delete selected (${selectedCount.value})` : "Delete"
))

function handleSelect() {
  emit("select", props.switchgear.id)
}

function handleContextMenu() {
  emit("contextSelect", props.switchgear.id)
}

function requestDelete() {
  if (usesSelectedDelete.value) {
    emit("deleteSelected")
    return
  }
  deleteOpen.value = true
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
  const before = [...switchgearStore.switchgears]
  const currentIndex = before.findIndex(item => item.id === props.switchgear.id)

  await switchgearStore.remove(props.switchgear.id)
  deleteOpen.value = false
  if (Number(route.params.id) === props.switchgear.id) {
    const after = switchgearStore.switchgears
    if (after.length === 0) {
      await router.push({ name: "switchgears.list" })
      return
    }

    const fallbackIndex = currentIndex < 0
      ? 0
      : Math.min(currentIndex, after.length - 1)
    const fallback = after[fallbackIndex]
    await router.push({ name: "switchgears.detail", params: { id: fallback.id } })
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
    <SidebarListItem
      :active="active"
      class="switchgear-list-item"
      :class="{ 'is-selected': selected }"
      @select="handleSelect"
      @contextmenu="handleContextMenu"
    >
      <span class="switchgear-list-item__title">
        {{ switchgear.name }}
      </span>
      <template #suffix>
        <span class="switchgear-list-item__position">
          <SwitchgearPositionIcon :state="positionState" />
        </span>
      </template>
      <template #subtitle>
        {{ switchgear.switchgear_type }}
      </template>
    </SidebarListItem>
    </UiMenuTrigger>
    <UiMenuContent>
      <UiMenuItem class="switchgear-list-item__menu-item" @select="openInNewTab">
        Open in new tab
      </UiMenuItem>
      <UiMenuItem class="switchgear-list-item__menu-item" @select="openRename">
        Rename
      </UiMenuItem>
      <UiMenuItem class="switchgear-list-item__menu-item" @select="duplicateSwitchgear">
        Duplicate
      </UiMenuItem>
      <UiMenuItem danger @select="requestDelete">
        {{ deleteMenuLabel }}
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

<style scoped>
.switchgear-list-item {
  position: relative;
  border-radius: var(--radius-md);
  background: transparent;
  box-shadow: inset 0 0 0 1px transparent;
  color: var(--color-neutral-700);
}

.switchgear-list-item.sidebar-list-item:not(.is-active):hover {
  background: color-mix(in srgb, var(--color-white) 72%, var(--color-neutral-100));
  box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--color-neutral-300) 82%, transparent);
  color: var(--color-neutral-900);
}

.switchgear-list-item.sidebar-list-item.is-active {
  background:
    linear-gradient(
      180deg,
      color-mix(in srgb, var(--color-blue-100) 52%, var(--color-white)),
      color-mix(in srgb, var(--color-white) 84%, var(--color-blue-100))
    );
  color: var(--color-neutral-900);
}

.switchgear-list-item.sidebar-list-item.is-active::after {
  position: absolute;
  inset: 0;
  border: 1px solid color-mix(in srgb, var(--color-blue-500) 34%, var(--color-neutral-200));
  border-radius: inherit;
  content: "";
  pointer-events: none;
}

.switchgear-list-item__title {
  display: block;
  min-width: 0;
  overflow: hidden;
  font-size: var(--text-sm);
  font-weight: 500;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.switchgear-list-item__position {
  display: inline-flex;
  flex: 0 0 auto;
  width: 1.5rem;
  align-items: center;
  justify-content: center;
}

.switchgear-list-item.sidebar-list-item:not(.is-active) :deep(.sidebar-list-item__indicator) {
  background: color-mix(in srgb, var(--color-neutral-300) 70%, transparent);
}

.switchgear-list-item__menu-item {
  color: var(--color-neutral-900);
}

:global(.dark .switchgear-list-item__menu-item) {
  color: var(--color-neutral-200);
}

:global(.dark .switchgear-list-item.sidebar-list-item) {
  background: transparent;
  box-shadow: inset 0 0 0 1px transparent;
  color: var(--color-neutral-200);
}

:global(.dark .switchgear-list-item.sidebar-list-item:not(.is-active):hover) {
  background: color-mix(in srgb, var(--color-neutral-900) 12%, transparent);
  box-shadow: inset 0 0 0 1px transparent;
  color: var(--color-white);
}

:global(.dark .switchgear-list-item.sidebar-list-item.is-active) {
  background: transparent;
  box-shadow: inset 0 0 0 1px transparent;
  color: var(--color-white);
}

:global(.dark .switchgear-list-item.sidebar-list-item.is-active::after) {
  border-color: transparent;
}

:global(.dark .switchgear-list-item.sidebar-list-item:not(.is-active) .sidebar-list-item__indicator) {
  background: color-mix(in srgb, var(--color-neutral-700) 78%, transparent);
}
</style>
