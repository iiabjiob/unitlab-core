<script setup lang="ts">
import { ref, computed } from "vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useRouter, useRoute } from "vue-router"
import SwitchgearListItem from "./SwitchgearListItem.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiSidebarListbox from "@/components/ui/UiSidebarListbox.vue"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useToastStore } from "@/stores/toastStore"
import { useSidebarBulkSelection } from "@/composables/useSidebarBulkSelection"
import type { Switchgear } from "@/types/switchgear"

const store = useSwitchgearStore()
const router = useRouter()
const route = useRoute()
const workspaceStore = useWorkspaceStore()
const toastStore = useToastStore()

function isActive(id: number) {
  return Number(route.params.id) === id
}

function openSwitchgear(id: number) {
  router.push({ name: "switchgears.detail", params: { id } })
}

async function addSwitchgear() {
  if (!workspaceStore.activeWorkspaceId) return
  const created = await store.createAuto()
  const assigned = (created.bindings ?? []).filter(binding => Number.isFinite(binding.channel_id as number)).length
  if (assigned < 4) {
    toastStore.warning(`Auto-allocation assigned ${assigned}/4 channels. Complete remaining bindings manually.`)
  }
  openSwitchgear(created.id)
}

// SEARCH
const query = ref("")
const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)

const filteredSwitchgears = computed(() => {
  if (workspaceMissing.value) return []
  if (!query.value.trim()) return store.switchgears

  const q = query.value.toLowerCase()

  return store.switchgears.filter(s =>
    s.name.toLowerCase().includes(q) ||
    s.switchgear_type.toLowerCase().includes(q)
  )
})

const selectedId = computed<number | null>(() => {
  const parsed = Number(route.params.id)
  return Number.isFinite(parsed) ? parsed : null
})

const {
  selectedIds,
  selectedCount,
  isSelected,
  handleSelection,
  prepareContextSelection,
  removeIds,
} = useSidebarBulkSelection(filteredSwitchgears)

const deleteSelectedOpen = ref(false)
const deletingSelected = ref(false)
const pendingDeleteIds = ref<number[]>([])

const selectedDeleteMessage = computed(() => {
  const count = pendingDeleteIds.value.length
  const suffix = count === 1 ? "switchgear" : "switchgears"
  return `${count} selected ${suffix} will be deleted.`
})

function handleSelect(id: string | number, event?: MouseEvent | KeyboardEvent) {
  const parsed = Number(id)
  if (!Number.isFinite(parsed)) return
  const action = handleSelection(parsed, event)
  if (action === "navigate") {
    openSwitchgear(parsed)
  }
}

function prepareItemContext(id: number) {
  prepareContextSelection(id)
}

function requestSelectedDelete() {
  if (deletingSelected.value) return
  const ids = selectedIds.value.length
    ? [...selectedIds.value]
    : selectedId.value !== null
      ? [selectedId.value]
      : []
  if (!ids.length) return
  pendingDeleteIds.value = ids
  deleteSelectedOpen.value = true
}

function cancelSelectedDelete() {
  deleteSelectedOpen.value = false
  pendingDeleteIds.value = []
}

async function confirmSelectedDelete() {
  const ids = [...pendingDeleteIds.value]
  if (!ids.length || deletingSelected.value) return

  const before = [...filteredSwitchgears.value]
  const deletedActive = selectedId.value === null || ids.includes(selectedId.value)
  const fallback = deletedActive ? resolveFallbackSwitchgear(before, ids) : null
  deletingSelected.value = true
  deleteSelectedOpen.value = false
  removeIds(ids)

  try {
    const deletion = store.removeMany(ids)
    if (deletedActive) {
      if (fallback) {
        await router.push({ name: "switchgears.detail", params: { id: fallback.id } })
      } else {
        await router.push({ name: "switchgears.list" })
      }
    }
    await deletion
    toastStore.success(ids.length === 1 ? "Switchgear deleted" : `${ids.length} switchgears deleted`)
  } catch (error) {
    toastStore.error(error instanceof Error ? error.message : "Failed to delete switchgears")
  } finally {
    deletingSelected.value = false
    pendingDeleteIds.value = []
  }
}

function resolveFallbackSwitchgear(before: Switchgear[], deletedIds: number[]): Switchgear | null {
  const deletedIdSet = new Set(deletedIds)
  const firstDeletedIndex = before.findIndex(item => deletedIdSet.has(item.id))
  const after = before.filter(item => !deletedIdSet.has(item.id))
  if (!after.length) {
    return null
  }

  const fallbackIndex = firstDeletedIndex < 0
    ? 0
    : Math.min(firstDeletedIndex, after.length - 1)
  return after[fallbackIndex] ?? null
}
</script>

<template>
  <div class="switchgear-list-sidebar">
    <div class="switchgear-list-sidebar__header">
      <UiButton
        variant="primary"
        size="sm"
        full
        :disabled="workspaceMissing"
        @click="addSwitchgear"
      >
        + New Switchgear
      </UiButton>
      <p
        v-if="workspaceMissing"
        class="switchgear-list-sidebar__workspace-hint"
      >
        Choose a workspace to start configuring
      </p>
    </div>

    <div class="switchgear-list-sidebar__search">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="switchgear-search"
        :disabled="workspaceMissing"
        :placeholder="workspaceMissing ? 'Select a workspace to get started' : 'Search switchgears…'"
        class="switchgear-list-sidebar__search-input"
      />
    </div>

    <div class="switchgear-list-sidebar__list">
      <div
        v-if="workspaceMissing"
        class="switchgear-list-sidebar__empty"
      >
        Switchgears belong to a workspace. Pick one to view its presets.
      </div>

      <template v-else>
        <UiSidebarListbox
          :items="filteredSwitchgears"
          :active-id="selectedId"
          :selected-ids="selectedIds"
          aria-label="Switchgears"
          @select="handleSelect"
          @delete="requestSelectedDelete"
        >
          <template #item="{ item: switchgear, isCursor }">
            <SwitchgearListItem
              :switchgear="switchgear"
              :active="isActive(switchgear.id) || isCursor || isSelected(switchgear.id)"
              :selected="isSelected(switchgear.id)"
              :selected-count="selectedCount"
              @context-select="prepareItemContext"
              @delete-selected="requestSelectedDelete"
            />
          </template>
          <template #empty>
            <div class="switchgear-list-sidebar__empty">
              No switchgears found
            </div>
          </template>
        </UiSidebarListbox>
      </template>
    </div>

  </div>

  <ConfirmModal
    :open="deleteSelectedOpen"
    title="Delete selected switchgears"
    :message="selectedDeleteMessage"
    confirm-label="Delete"
    cancel-label="Cancel"
    @cancel="cancelSelectedDelete"
    @confirm="confirmSelectedDelete"
  />
</template>

<style scoped>
.switchgear-list-sidebar {
  display: flex;
  height: 100%;
  flex-direction: column;
  gap: 0.75rem;
}

.switchgear-list-sidebar__header {
  padding: 0.75rem;
  border: 1px solid color-mix(in srgb, var(--runtime-accent) 16%, var(--color-neutral-200));
  border-radius: var(--radius-lg);
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--runtime-accent) 8%, var(--color-white)), color-mix(in srgb, var(--color-white) 88%, var(--color-neutral-100)));
  box-shadow: inset 0 1px 0 rgb(255 255 255 / 0.78);
}

.switchgear-list-sidebar__search {
  padding-bottom: 0.75rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 76%, transparent);
}

.switchgear-list-sidebar__workspace-hint {
  margin: 0.5rem 0 0;
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  letter-spacing: 0;
  text-transform: uppercase;
}

.switchgear-list-sidebar__search-input {
  width: 100%;
  padding: 0.5rem 0.75rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 72%, transparent);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 84%, var(--color-neutral-100));
  box-shadow: inset 0 1px 2px rgb(15 23 42 / 0.04);
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  outline: none;
}

.switchgear-list-sidebar__search-input:focus {
  border-color: color-mix(in srgb, var(--runtime-accent) 46%, var(--color-neutral-400));
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--runtime-accent) 16%, transparent);
}

.switchgear-list-sidebar__search-input::placeholder {
  color: var(--color-neutral-500);
}

.switchgear-list-sidebar__search-input:disabled {
  opacity: 0.6;
}

.switchgear-list-sidebar__list {
  flex: 1 1 auto;
  overflow-y: auto;
}

.switchgear-list-sidebar__list > :deep(* + *) {
  margin-top: 0.25rem;
}

.switchgear-list-sidebar__empty {
  padding: 1.5rem 1rem;
  border: 1px dashed color-mix(in srgb, var(--color-neutral-300) 70%, transparent);
  border-radius: 1rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  text-align: center;
}

:global(.dark .switchgear-list-sidebar__workspace-hint),
:global(.dark .switchgear-list-sidebar__empty) {
  color: var(--color-neutral-400);
}

:global(.dark .switchgear-list-sidebar__header) {
  border-color: color-mix(in srgb, var(--runtime-accent) 20%, var(--color-neutral-800));
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--runtime-accent) 10%, var(--color-neutral-900)), color-mix(in srgb, var(--color-neutral-950) 88%, var(--color-neutral-900)));
  box-shadow: inset 0 1px 0 rgb(255 255 255 / 0.04);
}

:global(.dark .switchgear-list-sidebar__search) {
  border-bottom-color: color-mix(in srgb, var(--color-neutral-800) 82%, transparent);
}

:global(.dark .switchgear-list-sidebar__search-input) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 78%, transparent);
  background: color-mix(in srgb, var(--color-neutral-950) 72%, var(--color-neutral-900));
  color: var(--color-neutral-100);
}

:global(.dark .switchgear-list-sidebar__search-input:focus) {
  border-color: color-mix(in srgb, var(--runtime-accent) 40%, var(--color-neutral-600));
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--runtime-accent) 18%, transparent);
}

:global(.dark .switchgear-list-sidebar__empty) {
  border-color: var(--color-neutral-700);
}
</style>
