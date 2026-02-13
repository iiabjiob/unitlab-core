<script setup lang="ts">
import { computed, ref } from "vue"
import { useRoute, useRouter } from "vue-router"
import type { SignalSnapshotSummary } from "@/types/signal"
import SidebarListItem from "@/components/ui/SidebarListItem.vue"
import UiBadge from "@/components/ui/UiBadge.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import { useSignalSnapshotStore } from "@/stores/signalSnapshotStore"
import { useToastStore } from "@/stores/toastStore"
import {
  UiMenu,
  UiMenuContent,
  UiMenuItem,
  type MenuController,
} from "@affino/menu-vue"

const props = defineProps<{
  snapshot: SignalSnapshotSummary
  active: boolean
}>()

const emit = defineEmits<{ (e: "select", id: number): void }>()
const snapshotStore = useSignalSnapshotStore()
const toastStore = useToastStore()
const router = useRouter()
const route = useRoute()
const deleteOpen = ref(false)
const menuRef = ref<{ controller?: MenuController } | null>(null)

const title = computed(() =>
  props.snapshot.source_filename ?? `Snapshot #${props.snapshot.id}`,
)

const meta = computed(() =>
  `${props.snapshot.rows_count} rows · schema v${props.snapshot.schema_version}`,
)

const statusVariant = computed(() =>
  props.snapshot.status === "locked" ? "success" : "warning",
)
const deleteMessage = computed(() => `Snapshot "${title.value}" will be deleted.`)

function handleSelect() {
  emit("select", props.snapshot.id)
}

async function lockSnapshot() {
  if (props.snapshot.status === "locked") return
  try {
    await snapshotStore.lockSnapshot(props.snapshot.id)
    toastStore.success("Snapshot locked")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function confirmDelete() {
  try {
    await snapshotStore.deleteSnapshot(props.snapshot.id)
    deleteOpen.value = false
    const currentSnapshot = Number(route.params.snapshotId)
    if (Number.isFinite(currentSnapshot) && currentSnapshot === props.snapshot.id) {
      await router.push({ name: "signals.home" })
    }
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

function openContextMenu(event: MouseEvent) {
  event.preventDefault()
  event.stopPropagation()
  const controller = menuRef.value?.controller
  if (!controller) return
  controller.setAnchor({ x: event.clientX, y: event.clientY, width: 0, height: 0 })
  controller.open("pointer")
}
</script>

<template>
  <UiMenu ref="menuRef">
    <SidebarListItem :active="active" class="relative pr-16" @select="handleSelect" @contextmenu="openContextMenu($event)">
      <span class="truncate text-sm font-medium">{{ title }}</span>
      <template #suffix>
        <span class="absolute right-3 top-1/2 -translate-y-1/2">
          <UiBadge :variant="statusVariant">
            {{ snapshot.status }}
          </UiBadge>
        </span>
      </template>
      <template #meta>
        {{ meta }}
      </template>
    </SidebarListItem>
    <UiMenuContent>
      <UiMenuItem v-if="snapshot.status !== 'locked'" class="text-neutral-900 dark:text-neutral-200" @select="lockSnapshot">
        Lock snapshot
      </UiMenuItem>
      <UiMenuItem danger @select="deleteOpen = true">
        Delete
      </UiMenuItem>
    </UiMenuContent>
  </UiMenu>

  <ConfirmModal
    :open="deleteOpen"
    title="Delete snapshot"
    :message="deleteMessage"
    confirm-label="Delete"
    cancel-label="Cancel"
    @cancel="deleteOpen = false"
    @confirm="confirmDelete"
  />
</template>
