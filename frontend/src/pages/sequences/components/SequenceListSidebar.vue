<script setup lang="ts">
import { ref, computed } from "vue"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useRouter, useRoute } from "vue-router"
import SequenceListItem from "./SequenceListItem.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiSidebarListbox from "@/components/ui/UiSidebarListbox.vue"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useSequenceImport } from "@/composables/useSequenceImport"
import { useToastStore } from "@/stores/toastStore"
import { useSidebarBulkSelection } from "@/composables/useSidebarBulkSelection"
import type { SequenceDef } from "@/types/sequences"

const store = useSequenceStore()
const router = useRouter()
const route = useRoute()
const workspaceStore = useWorkspaceStore()
const toastStore = useToastStore()
const { importing, fileInput, openFileDialog, onFileSelected } = useSequenceImport()

function isActive(id: number) {
  return Number(route.params.id) === id
}

function openSequence(id: number) {
  router.push(`/sequences/${id}`)
}

async function addSequence() {
  if (!workspaceStore.activeWorkspaceId) return
  try {
    const seq = await store.createSequenceAuto()
    await router.push(`/sequences/${seq.id}`)
  } catch (error) {
    toastStore.error(error instanceof Error ? error.message : "Failed to create instruction")
  }
}

// SEARCH
const query = ref("")
const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)

const filteredSequences = computed(() => {
  if (workspaceMissing.value) return []
  if (!query.value.trim()) return store.sequences

  const q = query.value.toLowerCase()

  return store.sequences.filter(s =>
    s.name.toLowerCase().includes(q) ||
    (s.description && s.description.toLowerCase().includes(q))
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
} = useSidebarBulkSelection(filteredSequences)

const deleteSelectedOpen = ref(false)
const deletingSelected = ref(false)
const pendingDeleteIds = ref<number[]>([])

const selectedDeleteMessage = computed(() => {
  const count = pendingDeleteIds.value.length
  const suffix = count === 1 ? "instruction" : "instructions"
  return `${count} selected ${suffix} will be deleted with all steps.`
})

function handleSelect(id: string | number, event?: MouseEvent | KeyboardEvent) {
  const parsed = Number(id)
  if (!Number.isFinite(parsed)) return
  const action = handleSelection(parsed, event)
  if (action === "navigate") {
    openSequence(parsed)
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

  const before = [...filteredSequences.value]
  const deletedActive = selectedId.value === null || ids.includes(selectedId.value)
  const fallback = deletedActive ? resolveFallbackSequence(before, ids) : null
  deletingSelected.value = true
  deleteSelectedOpen.value = false
  removeIds(ids)

  try {
    const deletion = store.deleteSequences(ids)
    if (deletedActive) {
      if (fallback) {
        await router.push({ name: "instructions.detail", params: { id: fallback.id } })
      } else {
        await router.push({ name: "instructions.list" })
      }
    }
    await deletion
    toastStore.success(ids.length === 1 ? "Instruction deleted" : `${ids.length} instructions deleted`)
  } catch (error) {
    toastStore.error(error instanceof Error ? error.message : "Failed to delete instructions")
  } finally {
    deletingSelected.value = false
    pendingDeleteIds.value = []
  }
}

function resolveFallbackSequence(before: SequenceDef[], deletedIds: number[]): SequenceDef | null {
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
  <div class="sequence-list-sidebar">

    <!-- HEADER -->
    <div class="sequence-list-sidebar__header">
      <UiButton
        variant="primary"
        size="sm"
        full
        :disabled="workspaceMissing"
        @click="addSequence"
      >
        + New Instruction
      </UiButton>
      <UiButton
        class="sequence-list-sidebar__import-button"
        variant="secondary"
        size="sm"
        full
        :disabled="workspaceMissing || importing"
        @click="openFileDialog"
      >
        {{ importing ? "📥 Importing…" : "📥 Import Instructions" }}
      </UiButton>
      <input
        ref="fileInput"
        class="sequence-list-sidebar__file-input"
        type="file"
        autocomplete="off"
        id="sequence-import-file"
        name="sequence-import-file"
        accept="application/json,.json"
        @change="onFileSelected"
      />
      <p
        v-if="workspaceMissing"
        class="sequence-list-sidebar__workspace-note"
      >
        Use the workspace switcher to enable edits
      </p>
    </div>

    <!-- SEARCH FIELD -->
    <div class="sequence-list-sidebar__search">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="instruction-search"
        :disabled="workspaceMissing"
        :placeholder="workspaceMissing ? 'Select a workspace to get started' : 'Search instructions…'"
        class="sequence-list-sidebar__input"
      />
    </div>

    <!-- LIST -->
    <div class="sequence-list-sidebar__list">
      <div
        v-if="workspaceMissing"
        class="sequence-list-sidebar__empty"
      >
        Select or create a workspace to see its instructions.
      </div>

      <template v-else>
        <UiSidebarListbox
          :items="filteredSequences"
          :active-id="selectedId"
          :selected-ids="selectedIds"
          aria-label="Instructions"
          @select="handleSelect"
          @delete="requestSelectedDelete"
        >
          <template #item="{ item: seq, isCursor }">
            <SequenceListItem
              :sequence="seq"
              :active="isActive(seq.id) || isCursor || isSelected(seq.id)"
              :selected="isSelected(seq.id)"
              :selected-count="selectedCount"
              @context-select="prepareItemContext"
              @delete-selected="requestSelectedDelete"
            />
          </template>
          <template #empty>
            <div class="sequence-list-sidebar__empty">
              No instructions found
            </div>
          </template>
        </UiSidebarListbox>
      </template>
    </div>

  </div>

  <ConfirmModal
    :open="deleteSelectedOpen"
    title="Delete selected instructions"
    :message="selectedDeleteMessage"
    confirm-label="Delete"
    cancel-label="Cancel"
    @cancel="cancelSelectedDelete"
    @confirm="confirmSelectedDelete"
  />
</template>

<style scoped>
.sequence-list-sidebar {
  display: flex;
  height: 100%;
  flex-direction: column;
  gap: 0.75rem;
}

.sequence-list-sidebar__header {
  padding: 0.75rem;
  border: 1px solid color-mix(in srgb, var(--runtime-accent) 16%, var(--color-neutral-200));
  border-radius: var(--radius-lg);
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--runtime-accent) 8%, var(--color-white)), color-mix(in srgb, var(--color-white) 88%, var(--color-neutral-100)));
  box-shadow: inset 0 1px 0 rgb(255 255 255 / 0.78);
}

.sequence-list-sidebar__import-button {
  margin-top: 0.5rem;
}

.sequence-list-sidebar__file-input {
  display: none;
}

.sequence-list-sidebar__workspace-note {
  margin-top: 0.5rem;
  color: var(--color-neutral-500);
  font-size: 11px;
  letter-spacing: 0;
  text-transform: uppercase;
}

.sequence-list-sidebar__search {
  padding-bottom: 0.75rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 76%, transparent);
}

.sequence-list-sidebar__input {
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

.sequence-list-sidebar__input::placeholder {
  color: var(--color-neutral-500);
}

.sequence-list-sidebar__input:focus {
  border-color: color-mix(in srgb, var(--runtime-accent) 46%, var(--color-neutral-400));
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--runtime-accent) 16%, transparent);
}

.sequence-list-sidebar__input:disabled {
  opacity: 0.6;
}

.sequence-list-sidebar__list {
  flex: 1 1 0%;
  overflow-y: auto;
}

.sequence-list-sidebar__list > :not(:last-child) {
  margin-bottom: 0.25rem;
}

.sequence-list-sidebar__empty {
  padding: 1.5rem 1rem;
  border: 1px dashed color-mix(in srgb, var(--color-neutral-300) 70%, transparent);
  border-radius: 1rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  text-align: center;
}

:global(.dark .sequence-list-sidebar__workspace-note) {
  color: var(--color-neutral-400);
}

:global(.dark .sequence-list-sidebar__header) {
  border-color: color-mix(in srgb, var(--runtime-accent) 20%, var(--color-neutral-800));
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--runtime-accent) 10%, var(--color-neutral-900)), color-mix(in srgb, var(--color-neutral-950) 88%, var(--color-neutral-900)));
  box-shadow: inset 0 1px 0 rgb(255 255 255 / 0.04);
}

:global(.dark .sequence-list-sidebar__search) {
  border-bottom-color: color-mix(in srgb, var(--color-neutral-800) 82%, transparent);
}

:global(.dark .sequence-list-sidebar__input) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 78%, transparent);
  background: color-mix(in srgb, var(--color-neutral-950) 72%, var(--color-neutral-900));
  color: var(--color-neutral-100);
}

:global(.dark .sequence-list-sidebar__input:focus) {
  border-color: color-mix(in srgb, var(--runtime-accent) 40%, var(--color-neutral-600));
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--runtime-accent) 18%, transparent);
}

:global(.dark .sequence-list-sidebar__empty) {
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-400);
}
</style>
