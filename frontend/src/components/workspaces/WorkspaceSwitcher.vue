<template>
  <div :class="[variantStyles.wrapper, 'workspace-switcher']">
    <UiMenu v-model:open="menuOpen">
      <UiMenuTrigger asChild>
        <button
          ref="triggerRef"
          type="button"
          :disabled="loading"
          :class="[triggerBaseClass, variantStyles.trigger]"
        >
          <div class="flex flex-col">
            <span :class="labelClass">Workspace</span>
            <span
              class="font-semibold"
              :class="[variantStyles.name, hasWorkspace ? 'text-neutral-900 dark:text-neutral-100' : 'text-neutral-500 dark:text-neutral-500']"
            >
              {{ currentLabel }}
            </span>
          </div>
          <div class="flex items-center gap-3 text-[11px] text-neutral-500 dark:text-neutral-400">
            <span>{{ workspaceStore.workspaces.length }} saved</span>
            <svg
              xmlns="http://www.w3.org/2000/svg"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="1.5"
              class="h-4 w-4"
            >
              <path stroke-linecap="round" stroke-linejoin="round" d="m6 9 6 6 6-6" />
            </svg>
          </div>
        </button>
      </UiMenuTrigger>

      <UiMenuContent class="border border-neutral-200 p-0 dark:border-neutral-800">
        <div class="border-b border-neutral-200 px-3 py-2 text-[10px] uppercase tracking-[0.3em] text-neutral-500 dark:border-neutral-800 dark:text-neutral-400">
          Workspaces
        </div>

        <div class="max-h-64 overflow-y-auto py-1">
          <template v-if="menuWorkspaces.length">
            <UiMenuItem
              v-for="workspace in menuWorkspaces"
              :key="workspace.id"
              class="flex items-center justify-between gap-4 px-3 py-2 text-sm"
              @select="select(workspace.id)"
            >
              <div class="flex flex-col">
                <span class="font-medium text-neutral-900 dark:text-neutral-100">{{ workspace.name }}</span>
                <span class="text-[11px] text-neutral-500 dark:text-neutral-400">
                  Updated {{ workspace.updated_at ? formatDateShort(workspace.updated_at) : "--" }}
                </span>
              </div>
              <span
                v-if="workspace.id === workspaceStore.activeWorkspaceId"
                class="text-[10px] uppercase tracking-[0.3em] text-emerald-500"
              >
                Active
              </span>
            </UiMenuItem>
          </template>
          <div v-else class="px-3 py-4 text-sm text-neutral-500 dark:text-neutral-400">
            No workspaces available.
          </div>
        </div>

        <UiMenuSeparator />

        <div class="px-3 py-3">
          <div class="flex flex-col gap-2">
            <button
              type="button"
              class="w-full rounded-md border border-dashed border-neutral-400 px-3 py-2 text-sm font-semibold text-neutral-900 transition hover:border-neutral-900 dark:border-neutral-600 dark:text-neutral-50 dark:hover:border-neutral-200"
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

const props = withDefaults(defineProps<{ variant?: "default" | "compact" | "mini" }>(), {
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

type WorkspaceSwitcherVariant = "default" | "compact" | "mini"

const VARIANT_STYLES: Record<WorkspaceSwitcherVariant, { wrapper: string; trigger: string; name: string }> = {
  default: {
    wrapper: "w-full max-w-2xl",
    trigger: "h-14 px-4 text-sm bg-white dark:bg-neutral-900",
    name: "text-base",
  },
  compact: {
    wrapper: "w-full",
    trigger: "h-12 px-4 text-xs bg-white dark:bg-neutral-900",
    name: "text-sm",
  },
  mini: {
    wrapper: "w-full",
    trigger: "h-12 px-3 text-sm bg-neutral-100 dark:bg-neutral-800",
    name: "text-lg",
  },
}

const triggerBaseClass = "flex w-full items-center justify-between rounded-lg border border-neutral-300 text-left text-neutral-900 dark:border-neutral-700 dark:text-neutral-100"
const variantStyles = computed(() => VARIANT_STYLES[props.variant])
const labelClass = "text-[10px] uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400"

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
