<template>
  <div class="h-full overflow-auto bg-neutral-50 dark:bg-neutral-950">
    <section class="mx-auto max-w-6xl px-6 py-10">
      <template v-if="activeProject">
        <header class="mb-8 rounded-2xl border border-neutral-200 bg-white px-6 py-6 text-neutral-900 shadow-sm dark:border-neutral-800 dark:bg-neutral-900 dark:text-neutral-50">
          <div class="flex flex-col gap-4 md:flex-row md:items-start md:justify-between">
            <div>
              <p class="text-[10px] uppercase tracking-[0.4em] text-neutral-500 dark:text-neutral-400">Active project</p>
              <div class="mt-3">
                <h1 class="text-3xl font-semibold tracking-tight">{{ activeProject.name }}</h1>
                <p class="text-sm text-neutral-500 dark:text-neutral-400">UUID {{ activeProject.uuid }}</p>
              </div>
            </div>
            <div class="flex items-start justify-end">
              <UiMenu>
                <UiMenuTrigger asChild>
                  <UiButton variant="icon" name="project-actions-button" aria-label="Project actions">
                    <EllipsisHorizontalIcon size="24" />
                  </UiButton>
                </UiMenuTrigger>
                <UiMenuContent>
                  <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="openRenameModal">
                    Rename
                  </UiMenuItem>
                  <UiMenuItem danger @select="openDeleteModal">
                    Delete
                  </UiMenuItem>
                </UiMenuContent>
              </UiMenu>
            </div>
          </div>
        </header>

        <div class="grid gap-4 md:grid-cols-4">
          <RouterLink
            v-for="metric in projectMetrics"
            :key="metric.label"
            :to="metric.to"
            class="block rounded-2xl border border-neutral-200 bg-white px-5 py-4 text-neutral-800 transition-colors hover:border-neutral-900 hover:bg-neutral-50 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-neutral-800 dark:border-neutral-800 dark:bg-neutral-900 dark:text-neutral-100 dark:hover:border-neutral-100 dark:hover:bg-neutral-800"
          >
            <p class="text-[10px] uppercase tracking-[0.4em] text-neutral-500 dark:text-neutral-400">{{ metric.label }}</p>
            <p class="mt-3 text-2xl font-mono">{{ metric.value }}</p>
          </RouterLink>
        </div>
      </template>

      <template v-else>
        <div class="flex flex-col items-center gap-6 rounded-2xl border border-dashed border-neutral-300 bg-white px-6 py-14 text-center text-neutral-700 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200">
          <AppLogo class="h-16 w-16" />
          <p class="text-sm uppercase tracking-[0.5em] text-neutral-500 dark:text-neutral-400">Select project</p>
          <p class="text-base text-neutral-600 dark:text-neutral-300">Choose any of the recent workspaces below to view metrics and begin configuration.</p>
        </div>

        <div class="mt-8 rounded-2xl border border-neutral-200 bg-white px-5 py-5 dark:border-neutral-800 dark:bg-neutral-900">
          <p class="text-[10px] uppercase tracking-[0.4em] text-neutral-500 dark:text-neutral-400">Recent projects</p>
          <div class="mt-4 divide-y divide-neutral-200 dark:divide-neutral-800">
            <button
              v-for="project in recentProjects"
              :key="project.id"
              class="flex w-full items-center justify-between gap-6 px-2 py-3 text-left text-sm text-neutral-800 transition hover:bg-neutral-50 dark:text-neutral-100 dark:hover:bg-neutral-800"
              @click="projectStore.selectProject(project.id)"
            >
              <div>
                <p class="font-semibold">{{ project.name }}</p>
                <p class="text-xs text-neutral-500 dark:text-neutral-400">Updated {{ formatTimestamp(project.updated_at) }}</p>
              </div>
              <span class="text-[10px] uppercase tracking-[0.4em] text-neutral-500">Open</span>
            </button>

            <div v-if="!recentProjects.length" class="py-4 text-sm text-neutral-500 dark:text-neutral-400">
              No projects yet. Use the switcher to create one.
            </div>
          </div>
        </div>
      </template>
    </section>

    <RenameModal
      :open="renameModalOpen"
      title="Rename project"
      v-model="renameValue"
      :loading="isRenaming"
      :error="renameError"
      @cancel="handleRenameCancel"
      @confirm="submitRename"
    />

    <ConfirmModal
      :open="deleteModalOpen"
      title="Delete project"
      :message="deleteModalMessage"
      cancel-label="Cancel"
      :confirm-label="deleteConfirmLabel"
      :enter-confirms="false"
      @cancel="handleDeleteCancel"
      @confirm="confirmDelete"
    />
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"
import type { RouteLocationRaw } from "vue-router"
import AppLogo from "@/components/layout/AppLogo.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import RenameModal from "@/components/ui/RenameModal.vue"
import UiButton from "@/components/ui/UiButton.vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@affino/menu-vue"
import EllipsisHorizontalIcon from "@/components/icons/EllipsisHorizontalIcon.vue"
import { useProjectStore } from "@/stores/projectStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { formatTsFull } from "@/utils/datetime"

const projectStore = useProjectStore()
const sequenceStore = useSequenceStore()
const switchgearStore = useSwitchgearStore()

onMounted(() => {
  void projectStore.bootstrap()
})

const activeProject = computed(() => projectStore.activeProject)

watch(
  () => projectStore.activeProjectId,
  (id) => {
    if (id) {
      void sequenceStore.ensureLoaded()
      void switchgearStore.ensureLoaded()
    }
  },
  { immediate: true },
)

const renameModalOpen = ref(false)
const renameValue = ref("")
const renameError = ref("")
const isRenaming = ref(false)

const deleteModalOpen = ref(false)
const deleteError = ref("")
const isDeleting = ref(false)

watch(activeProject, (project) => {
  if (!project) {
    renameModalOpen.value = false
    deleteModalOpen.value = false
    renameValue.value = ""
    renameError.value = ""
    deleteError.value = ""
  }
})

function formatTimestamp(value?: string | null) {
  if (!value) return "--"
  const ms = new Date(value).getTime()
  if (Number.isNaN(ms)) return "--"
  return formatTsFull(ms)
}

type MetricCard = {
  label: string
  value: number | string
  to: RouteLocationRaw
}

const projectMetrics = computed<MetricCard[]>(() => {
  if (!activeProject.value) return []
  return [
    { label: "Sequences", value: sequenceStore.sequences.length, to: { name: "sequences.list" } },
    { label: "Switchgears", value: switchgearStore.switchgears.length, to: { name: "switchgears.list" } },
  ]
})

const recentProjects = computed(() => projectStore.projects.slice(0, 5))

function openRenameModal() {
  if (!activeProject.value || isRenaming.value) return
  renameValue.value = activeProject.value.name
  renameError.value = ""
  renameModalOpen.value = true
}

function handleRenameCancel() {
  if (isRenaming.value) return
  renameError.value = ""
  renameValue.value = activeProject.value?.name ?? ""
  renameModalOpen.value = false
}

async function submitRename() {
  if (!activeProject.value || isRenaming.value) return
  const nextName = renameValue.value.trim()
  if (!nextName) {
    renameError.value = "Name is required"
    return
  }
  if (nextName === activeProject.value.name) {
    renameModalOpen.value = false
    return
  }

  renameError.value = ""
  isRenaming.value = true
  try {
    await projectStore.renameProject(activeProject.value.id, nextName)
    renameModalOpen.value = false
  } catch (error) {
    renameError.value = error instanceof Error ? error.message : "Failed to rename project"
  } finally {
    isRenaming.value = false
  }
}

function openDeleteModal() {
  if (!activeProject.value || isDeleting.value) return
  deleteError.value = ""
  deleteModalOpen.value = true
}

function handleDeleteCancel() {
  if (isDeleting.value) return
  deleteModalOpen.value = false
}

const deleteModalMessage = computed(() => {
  if (!activeProject.value) return ""
  const base = `Deleting "${activeProject.value.name}" will remove all resources under this project. This action cannot be undone.`
  return deleteError.value ? `${base} ${deleteError.value}` : base
})

const deleteConfirmLabel = computed(() => (isDeleting.value ? "Deleting..." : "Delete"))

async function confirmDelete() {
  if (!activeProject.value || isDeleting.value) return
  deleteError.value = ""
  isDeleting.value = true
  try {
    await projectStore.deleteProject(activeProject.value.id)
    deleteModalOpen.value = false
  } catch (error) {
    deleteError.value = error instanceof Error ? error.message : "Failed to delete project"
  } finally {
    isDeleting.value = false
  }
}
</script>
