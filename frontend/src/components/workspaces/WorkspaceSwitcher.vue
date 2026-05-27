<template>
  <div class="workspace-switcher" :class="variantClass">
    <UiMenu v-model:open="menuOpen">
      <UiMenuTrigger asChild>
        <button
          ref="triggerRef"
          type="button"
          :disabled="loading"
          class="workspace-switcher__trigger"
          :class="triggerClass"
        >
          <div class="workspace-switcher__label-stack">
            <span class="workspace-switcher__label">Workspace</span>
            <span
              class="workspace-switcher__name"
              :class="[{ 'workspace-switcher__name--empty': !hasWorkspace }, nameClass]"
            >
              {{ currentLabel }}
            </span>
          </div>
          <div class="workspace-switcher__summary">
            <span>{{ workspaceStore.workspaces.length }} saved</span>
            <svg
              xmlns="http://www.w3.org/2000/svg"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="1.5"
              class="workspace-switcher__chevron"
            >
              <path stroke-linecap="round" stroke-linejoin="round" d="m6 9 6 6 6-6" />
            </svg>
          </div>
        </button>
      </UiMenuTrigger>

      <UiMenuContent class="workspace-switcher__menu">
        <div class="workspace-switcher__menu-label">
          Workspaces
        </div>

        <div class="workspace-switcher__list">
          <template v-if="menuWorkspaces.length">
            <UiMenuItem
              v-for="workspace in menuWorkspaces"
              :key="workspace.id"
              class="workspace-switcher__item"
              @select="select(workspace.id)"
            >
              <div class="workspace-switcher__item-text">
                <span class="workspace-switcher__item-name">{{ workspace.name }}</span>
                <span class="workspace-switcher__item-meta">
                  Updated {{ workspace.updated_at ? formatDateShort(workspace.updated_at) : "--" }}
                </span>
              </div>
              <span
                v-if="workspace.id === workspaceStore.activeWorkspaceId"
                class="workspace-switcher__active-label"
              >
                Active
              </span>
            </UiMenuItem>
          </template>
          <div v-else class="workspace-switcher__empty">
            No workspaces available.
          </div>
        </div>

        <UiMenuSeparator />

        <div class="workspace-switcher__actions">
          <div class="workspace-switcher__action-stack">
            <button
              type="button"
              class="workspace-switcher__new-button"
              @click.stop="openCreateModal"
            >
              + New workspace
            </button>
            <UiButton
              type="button"
              variant="danger"
              size="sm"
              :disabled="!canDeleteActiveWorkspace || isDeleting"
              @click.stop="openDeleteModal"
            >
              Delete active workspace
            </UiButton>
          </div>
        </div>
      </UiMenuContent>
    </UiMenu>

    <RenameModal
      :open="createModalOpen"
      title="Create workspace"
      label="Workspace name"
      confirm-label="Create"
      v-model="createName"
      :loading="isCreating"
      :error="createError"
      @cancel="handleCreateCancel"
      @confirm="handleCreateSubmit"
    />

    <ConfirmModal
      :open="deleteModalOpen"
      title="Delete workspace"
      :message="deleteModalMessage"
      confirm-label="Delete"
      cancel-label="Cancel"
      @cancel="handleDeleteCancel"
      @confirm="handleDeleteConfirm"
    />
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from "vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
  UiMenuSeparator,
} from "@/components/ui/menu"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useToastStore } from "@/stores/toastStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"
import RenameModal from "@/components/ui/RenameModal.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import UiButton from "@/components/ui/UiButton.vue"
import { formatDateShort } from "@/utils/datetime"

type WorkspaceSwitcherVariant = "default" | "compact" | "mini"

const props = withDefaults(defineProps<{ variant?: WorkspaceSwitcherVariant }>(), {
  variant: "default",
})

const workspaceStore = useWorkspaceStore()
const toastStore = useToastStore()
const isCreating = ref(false)
const menuOpen = ref(false)
const createModalOpen = ref(false)
const createName = ref("")
const createError = ref("")
const deleteModalOpen = ref(false)
const isDeleting = ref(false)

const variantClass = computed(() => `workspace-switcher--${props.variant}`)
const triggerClass = computed(() => `workspace-switcher__trigger--${props.variant}`)
const nameClass = computed(() => `workspace-switcher__name--${props.variant}`)

const loading = computed(() => workspaceStore.loading)
const hasWorkspace = computed(() => Boolean(workspaceStore.activeWorkspace))
const currentLabel = computed(() => workspaceStore.activeWorkspace?.name ?? "Select workspace")
const menuWorkspaces = computed(() => workspaceStore.workspaces)
const canDeleteActiveWorkspace = computed(() => Boolean(workspaceStore.activeWorkspaceId))
const deleteModalMessage = computed(() => {
  const name = workspaceStore.activeWorkspace?.name ?? "this workspace"
  return `Workspace "${name}" will be deleted.`
})

async function select(workspaceId: number) {
  await workspaceStore.selectWorkspace(workspaceId)
  menuOpen.value = false
}

function openCreateModal() {
  if (isCreating.value) return
  createName.value = ""
  createError.value = ""
  createModalOpen.value = true
  menuOpen.value = false
}

function openDeleteModal() {
  if (!canDeleteActiveWorkspace.value || isDeleting.value) return
  deleteModalOpen.value = true
  menuOpen.value = false
}

function handleCreateCancel() {
  if (isCreating.value) return
  createModalOpen.value = false
  createError.value = ""
}

async function handleCreateSubmit() {
  if (isCreating.value) return
  const trimmed = createName.value.trim()
  if (!trimmed) {
    createError.value = "Name is required"
    return
  }

  createError.value = ""
  isCreating.value = true
  try {
    await workspaceStore.createWorkspace(trimmed)
    createModalOpen.value = false
  } catch (error) {
    createError.value = error instanceof Error ? error.message : "Failed to create workspace"
  } finally {
    isCreating.value = false
  }
}

function handleDeleteCancel() {
  if (isDeleting.value) return
  deleteModalOpen.value = false
}

async function handleDeleteConfirm() {
  const workspaceId = workspaceStore.activeWorkspaceId
  if (!workspaceId || isDeleting.value) {
    deleteModalOpen.value = false
    return
  }

  isDeleting.value = true
  try {
    const workspaceName = workspaceStore.activeWorkspace?.name ?? "Workspace"
    await workspaceStore.deleteWorkspace(workspaceId)
    deleteModalOpen.value = false
    toastStore.success(`${workspaceName} deleted`)
  } catch (error) {
    const message = error instanceof Error ? error.message : "Failed to delete workspace"
    toastStore.error(message)
  } finally {
    isDeleting.value = false
  }
}

onMounted(() => {
  void runStoreBootstrap(
    ["workspace-switcher-bootstrap"],
    [() => workspaceStore.bootstrap()],
    { mode: "settled" },
  )
})

</script>

<style scoped>
.workspace-switcher {
  width: 100%;
}

.workspace-switcher--default {
  max-width: 42rem;
}

.workspace-switcher__trigger {
  display: flex;
  width: 100%;
  align-items: center;
  justify-content: space-between;
  border: 1px solid var(--color-neutral-300);
  border-radius: 0.5rem;
  font: inherit;
  color: var(--color-neutral-900);
  text-align: left;
}

.workspace-switcher__trigger:disabled {
  opacity: 0.65;
}

.workspace-switcher__trigger--default {
  height: 3.5rem;
  padding: 0 1rem;
  background: var(--color-white);
  font-size: var(--text-sm);
}

.workspace-switcher__trigger--compact {
  height: 3rem;
  padding: 0 1rem;
  background: var(--color-white);
  font-size: var(--text-xs);
}

.workspace-switcher__trigger--mini {
  height: 3rem;
  padding: 0 0.75rem;
  background: var(--color-neutral-100);
  font-size: var(--text-sm);
}

.workspace-switcher__label-stack {
  display: flex;
  flex-direction: column;
}

.workspace-switcher__label {
  color: var(--color-neutral-500);
  font-size: 0.625rem;
  letter-spacing: 0;
  text-transform: uppercase;
}

.workspace-switcher__name {
  color: var(--color-neutral-900);
  font-weight: 600;
}

.workspace-switcher__name--default {
  font-size: var(--text-base);
}

.workspace-switcher__name--compact {
  font-size: var(--text-sm);
}

.workspace-switcher__name--mini {
  font-size: var(--text-lg);
}

.workspace-switcher__name--empty {
  color: var(--color-neutral-500);
}

.workspace-switcher__summary {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
}

.workspace-switcher__chevron {
  width: 1rem;
  height: 1rem;
}

.workspace-switcher__menu {
  padding: 0;
  border: 1px solid var(--color-neutral-200);
}

.workspace-switcher__menu-label {
  padding: 0.5rem 0.75rem;
  border-bottom: 1px solid var(--color-neutral-200);
  color: var(--color-neutral-500);
  font-size: 0.625rem;
  letter-spacing: 0;
  text-transform: uppercase;
}

.workspace-switcher__list {
  max-height: 16rem;
  overflow-y: auto;
  padding: 0.25rem 0;
}

.workspace-switcher__item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
  padding: 0.5rem 0.75rem;
  font-size: var(--text-sm);
}

.workspace-switcher__item-text {
  display: flex;
  flex-direction: column;
}

.workspace-switcher__item-name {
  color: var(--color-neutral-900);
  font-weight: 500;
}

.workspace-switcher__item-meta {
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
}

.workspace-switcher__active-label {
  color: var(--color-emerald-500);
  font-size: 0.625rem;
  letter-spacing: 0;
  text-transform: uppercase;
}

.workspace-switcher__empty {
  padding: 1rem 0.75rem;
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

.workspace-switcher__actions {
  padding: 0.75rem;
}

.workspace-switcher__action-stack {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.workspace-switcher__new-button {
  width: 100%;
  padding: 0.5rem 0.75rem;
  border: 1px dashed var(--color-neutral-400);
  border-radius: var(--radius-md);
  background: transparent;
  color: var(--color-neutral-900);
  font: inherit;
  font-size: var(--text-sm);
  font-weight: 600;
  transition: border-color 0.15s ease;
}

.workspace-switcher__new-button:hover {
  border-color: var(--color-neutral-900);
}

:global(.dark .workspace-switcher__trigger) {
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-100);
}

:global(.dark .workspace-switcher__trigger--default),
:global(.dark .workspace-switcher__trigger--compact) {
  background: var(--color-neutral-900);
}

:global(.dark .workspace-switcher__trigger--mini) {
  background: var(--color-neutral-800);
}

:global(.dark .workspace-switcher__label),
:global(.dark .workspace-switcher__summary),
:global(.dark .workspace-switcher__item-meta),
:global(.dark .workspace-switcher__empty) {
  color: var(--color-neutral-400);
}

:global(.dark .workspace-switcher__name),
:global(.dark .workspace-switcher__item-name) {
  color: var(--color-neutral-100);
}

:global(.dark .workspace-switcher__name--empty) {
  color: var(--color-neutral-500);
}

:global(.dark .workspace-switcher__menu) {
  border-color: var(--color-neutral-800);
}

:global(.dark .workspace-switcher__menu-label) {
  border-bottom-color: var(--color-neutral-800);
  color: var(--color-neutral-400);
}

:global(.dark .workspace-switcher__new-button) {
  border-color: var(--color-neutral-600);
  color: var(--color-neutral-50);
}

:global(.dark .workspace-switcher__new-button:hover) {
  border-color: var(--color-neutral-200);
}
</style>
