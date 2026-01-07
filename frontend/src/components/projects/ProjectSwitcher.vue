<template>
  <div :class="[wrapperClass, 'project-switcher']">
    <UiMenu v-model:open="menuOpen">
      <UiMenuTrigger asChild>
        <button
          type="button"
          :disabled="loading"
          :class="triggerClasses"
        >
          <div class="flex flex-col">
            <span :class="labelClass">Project</span>
            <span
              class="font-semibold"
              :class="[nameClass, hasProject ? 'text-neutral-900 dark:text-neutral-100' : 'text-neutral-500 dark:text-neutral-500']"
            >
              {{ currentLabel }}
            </span>
          </div>
          <div class="flex items-center gap-3 text-[11px] text-neutral-500 dark:text-neutral-400">
            <span>{{ projectStore.projects.length }} saved</span>
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

      <UiMenuContent class="min-w-[320px] border border-neutral-200 p-0 dark:border-neutral-800">
        <div class="border-b border-neutral-200 px-3 py-2 text-[10px] uppercase tracking-[0.3em] text-neutral-500 dark:border-neutral-800 dark:text-neutral-400">
          Projects
        </div>

        <div class="max-h-64 overflow-y-auto py-1">
          <template v-if="menuProjects.length">
            <UiMenuItem
              v-for="project in menuProjects"
              :key="project.id"
              class="flex items-center justify-between gap-4 px-3 py-2 text-sm"
              @select="select(project.id)"
            >
              <div class="flex flex-col">
                <span class="font-medium text-neutral-900 dark:text-neutral-100">{{ project.name }}</span>
                <span class="text-[11px] text-neutral-500 dark:text-neutral-400">
                  Updated {{ formatTimestamp(project.updated_at) }}
                </span>
              </div>
              <span
                v-if="project.id === projectStore.activeProjectId"
                class="text-[10px] uppercase tracking-[0.3em] text-emerald-500"
              >
                Active
              </span>
            </UiMenuItem>
          </template>
          <div v-else class="px-3 py-4 text-sm text-neutral-500 dark:text-neutral-400">
            No projects available.
          </div>
        </div>

        <UiMenuSeparator />

        <div class="px-3 py-3">
          <button
            type="button"
            class="w-full rounded-md border border-dashed border-neutral-400 px-3 py-2 text-sm font-semibold text-neutral-900 transition hover:border-neutral-900 dark:border-neutral-600 dark:text-neutral-50 dark:hover:border-neutral-200"
            @click.stop="openCreateModal"
          >
            + New project
          </button>
        </div>
      </UiMenuContent>
    </UiMenu>

    <RenameModal
      :open="createModalOpen"
      title="Create project"
      label="Project name"
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
import { computed, onMounted, ref } from "vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
  UiMenuSeparator,
} from "@affino/menu-vue"
import { useProjectStore } from "@/stores/projectStore"
import { formatTsFull } from "@/utils/datetime"
import RenameModal from "@/components/ui/RenameModal.vue"

const props = withDefaults(defineProps<{ variant?: "default" | "compact" | "mini" }>(), {
  variant: "default",
})

const projectStore = useProjectStore()
const isCreating = ref(false)
const menuOpen = ref(false)
const createModalOpen = ref(false)
const createName = ref("")
const createError = ref("")

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

const loading = computed(() => projectStore.loading)
const hasProject = computed(() => Boolean(projectStore.activeProject))
const currentLabel = computed(() => projectStore.activeProject?.name ?? "Select project")
const menuProjects = computed(() => projectStore.projects)

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

async function select(projectId: number) {
  await projectStore.selectProject(projectId)
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
    await projectStore.createProject(trimmed)
    createModalOpen.value = false
  } catch (error) {
    createError.value = error instanceof Error ? error.message : "Failed to create project"
  } finally {
    isCreating.value = false
  }
}

onMounted(() => {
  void projectStore.bootstrap()
})
</script>
