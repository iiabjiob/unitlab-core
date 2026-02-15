<template>
  <div :class="[wrapperClass, 'workspace-switcher']">
    <UiMenu v-model:open="menuOpen">
      <UiMenuTrigger asChild>
        <button
          ref="triggerRef"
          type="button"
          :disabled="loading"
          :class="triggerClasses"
        >
          <div class="flex flex-col">
            <span :class="labelClass">Workspace</span>
            <span
              class="font-semibold"
              :class="[nameClass, hasWorkspace ? 'text-neutral-900 dark:text-neutral-100' : 'text-neutral-500 dark:text-neutral-500']"
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

      <UiMenuContent :style="menuContentStyle" class="border border-neutral-200 p-0 dark:border-neutral-800">
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
                  Updated {{ formatTimestamp(workspace.updated_at) }}
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
          <button
            type="button"
            class="w-full rounded-md border border-dashed border-neutral-400 px-3 py-2 text-sm font-semibold text-neutral-900 transition hover:border-neutral-900 dark:border-neutral-600 dark:text-neutral-50 dark:hover:border-neutral-200"
            @click.stop="openCreateModal"
          >
            + New workspace
          </button>
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
  </div>
</template>

<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
  UiMenuSeparator,
} from "@affino/menu-vue"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import RenameModal from "@/components/ui/RenameModal.vue"

const props = withDefaults(defineProps<{ variant?: "default" | "compact" | "mini" }>(), {
  variant: "default",
})

const workspaceStore = useWorkspaceStore()
const isCreating = ref(false)
const menuOpen = ref(false)
const createModalOpen = ref(false)
const createName = ref("")
const createError = ref("")
const triggerRef = ref<HTMLElement | null>(null)
const menuWidth = ref<number | null>(null)
let triggerResizeObserver: ResizeObserver | null = null

const wrapperClass = computed(() => {
  if (props.variant === "compact") return "w-full"
  if (props.variant === "mini") return "w-full"
  return "w-full max-w-2xl"
})
const triggerClasses = computed(() => {
  const classes = [
    "flex w-full items-center justify-between rounded-lg border border-neutral-300 text-left text-neutral-900 dark:border-neutral-700 dark:text-neutral-100",
  ]

  if (props.variant === "compact") {
    classes.push("h-12 px-4 text-xs bg-white dark:bg-neutral-900")
  } else if (props.variant === "mini") {
    classes.push("h-12 px-3 text-sm bg-neutral-100 dark:bg-neutral-800")
  } else {
    classes.push("h-14 px-4 text-sm bg-white dark:bg-neutral-900")
  }

  return classes
})
const nameClass = computed(() => {
  if (props.variant === "mini") return "text-lg"
  if (props.variant === "compact") return "text-sm"
  return "text-base"
})
const labelClass = "text-[10px] uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400"

const loading = computed(() => workspaceStore.loading)
const hasWorkspace = computed(() => Boolean(workspaceStore.activeWorkspace))
const currentLabel = computed(() => workspaceStore.activeWorkspace?.name ?? "Select workspace")
const menuWorkspaces = computed(() => workspaceStore.workspaces)
const menuContentStyle = computed<Record<string, string> | undefined>(() => {
  if (!menuWidth.value) return undefined
  const widthPx = `${menuWidth.value}px`
  return {
    width: widthPx,
    minWidth: widthPx,
  }
})

function updateMenuWidth() {
  const el = triggerRef.value
  if (!el) return
  const width = Math.max(0, Math.round(el.getBoundingClientRect().width))
  menuWidth.value = width > 0 ? width : null
}

function formatTimestamp(value?: string | null) {
  if (!value) return "--"
  const d = new Date(value)
  if (Number.isNaN(d.getTime())) return "--"
  return d.toLocaleDateString("en-GB", {
    year: "numeric",
    month: "short",
    day: "2-digit",
  })
}

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

onMounted(() => {
  void nextTick(() => {
    updateMenuWidth()
  })
  if (typeof ResizeObserver !== "undefined" && triggerRef.value) {
    triggerResizeObserver = new ResizeObserver(() => {
      updateMenuWidth()
    })
    triggerResizeObserver.observe(triggerRef.value)
  }
  void workspaceStore.bootstrap()
})

onBeforeUnmount(() => {
  triggerResizeObserver?.disconnect()
  triggerResizeObserver = null
})

watch(
  () => [menuOpen.value, props.variant, currentLabel.value, menuWorkspaces.value.length] as const,
  () => {
    void nextTick(() => {
      updateMenuWidth()
    })
  },
)
</script>
