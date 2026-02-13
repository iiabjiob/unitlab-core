<template>
  <div class="h-full flex flex-col">

    <div class="mb-3 space-y-2">
      <div class="flex items-center gap-2">
        <UiButton variant="primary" size="sm" full :disabled="workspaceMissing" @click="emit('import')">
          + Import Signal List
        </UiButton>
        <UiMenu v-if="selectedSnapshot">
          <UiMenuTrigger asChild>
            <UiButton
              variant="icon"
              aria-label="Snapshot actions"
              @click.stop
              @pointerdown.stop
            >
              <EllipsisHorizontalIcon size="20" />
            </UiButton>
          </UiMenuTrigger>
          <UiMenuContent>
            <UiMenuItem v-if="selectedSnapshot.status !== 'locked'" class="text-neutral-900 dark:text-neutral-200" @select="lockSelectedSnapshot">
              Lock snapshot
            </UiMenuItem>
            <UiMenuItem danger @select="deleteOpen = true">
              Delete
            </UiMenuItem>
          </UiMenuContent>
        </UiMenu>
      </div>
      <p
        v-if="workspaceMissing"
        class="mt-2 text-[11px] uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400"
      >
        Choose a workspace to view snapshots
      </p>
    </div>

    <div class="mb-3">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="snapshot-search"
        :disabled="workspaceMissing"
        :placeholder="workspaceMissing ? 'Select a workspace to get started' : 'Search snapshots…'"
        class="w-full rounded-lg border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none disabled:opacity-60 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
      />
    </div>

    <div class="flex-1 overflow-y-auto space-y-1">
      <div
        v-if="workspaceMissing"
        class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
      >
        Snapshots belong to a workspace. Pick one to manage signal imports.
      </div>

      <template v-else>
        <div
          v-if="loading"
          class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
        >
          Loading snapshots…
        </div>

        <template v-else>
          <div
            v-if="truncatedList"
            class="rounded-2xl border border-neutral-200/70 px-3 py-2 text-[11px] uppercase tracking-[0.2em] text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
          >
            Showing latest {{ MAX_RENDERED_SNAPSHOTS }} snapshots
          </div>

          <UiSidebarListbox
            :items="filteredSnapshots"
            :active-id="selectedId"
            aria-label="Signal snapshots"
            @select="handleSelect"
          >
            <template #item="{ item: snapshot, isCursor }">
              <SignalListItem
                :snapshot="snapshot"
                :active="snapshot.id === selectedId || isCursor"
              />
            </template>
            <template #empty>
              <div class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
                No snapshots found
              </div>
            </template>
          </UiSidebarListbox>
        </template>
      </template>
    </div>

    <ConfirmModal
      :open="deleteOpen"
      title="Delete snapshot"
      :message="deleteMessage"
      confirm-label="Delete"
      cancel-label="Cancel"
      @cancel="deleteOpen = false"
      @confirm="confirmDeleteSelected"
    />
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"
import { useRouter } from "vue-router"

import UiButton from "@/components/ui/UiButton.vue"
import UiSidebarListbox from "@/components/ui/UiSidebarListbox.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import SignalListItem from "./SignalListItem.vue"
import type { SignalSnapshotSummary } from "@/types/signal"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useSignalSnapshotStore } from "@/stores/signalSnapshotStore"
import { useToastStore } from "@/stores/toastStore"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@affino/menu-vue"
import EllipsisHorizontalIcon from "@/components/icons/EllipsisHorizontalIcon.vue"

const props = defineProps<{
  snapshots: SignalSnapshotSummary[]
  loading?: boolean
  selectedId: number | null
}>()

const emit = defineEmits<{ (e: "select", id: number): void; (e: "import"): void }>()

const workspaceStore = useWorkspaceStore()
const snapshotStore = useSignalSnapshotStore()
const toastStore = useToastStore()
const router = useRouter()
const query = ref("")
const MAX_RENDERED_SNAPSHOTS = 200
const deleteOpen = ref(false)

const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)
const visibleSnapshots = computed(() => props.snapshots.slice(0, MAX_RENDERED_SNAPSHOTS))
const truncatedList = computed(() => props.snapshots.length > MAX_RENDERED_SNAPSHOTS)
const selectedSnapshot = computed(() => {
  if (props.selectedId === null) return null
  return props.snapshots.find((snapshot) => snapshot.id === props.selectedId) ?? null
})
const deleteMessage = computed(() => {
  if (!selectedSnapshot.value) return ""
  const name = selectedSnapshot.value.source_filename ?? `Snapshot #${selectedSnapshot.value.id}`
  return `Snapshot "${name}" will be deleted.`
})

const filteredSnapshots = computed(() => {
  if (workspaceMissing.value) return []
  if (!query.value.trim()) return visibleSnapshots.value
  const q = query.value.toLowerCase()
  return visibleSnapshots.value.filter(snapshot => {
    const title = snapshot.source_filename ?? `snapshot-${snapshot.id}`
    return title.toLowerCase().includes(q) || (snapshot.source_hash?.toLowerCase().includes(q) ?? false)
  })
})

const loading = computed(() => props.loading ?? false)

function handleSelect(id: string | number) {
  const parsed = Number(id)
  if (!Number.isFinite(parsed)) return
  emit("select", parsed)
}

async function lockSelectedSnapshot() {
  if (!selectedSnapshot.value || selectedSnapshot.value.status === "locked") return
  try {
    await snapshotStore.lockSnapshot(selectedSnapshot.value.id)
    toastStore.success("Snapshot locked")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function confirmDeleteSelected() {
  if (!selectedSnapshot.value) return
  try {
    await snapshotStore.deleteSnapshot(selectedSnapshot.value.id)
    deleteOpen.value = false
    if (props.selectedId === selectedSnapshot.value.id) {
      await router.push({ name: "signals.home" })
    }
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}
</script>
