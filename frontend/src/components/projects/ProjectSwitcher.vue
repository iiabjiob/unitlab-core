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

        <form class="space-y-2 px-3 py-3" @submit.prevent="handleCreate" @click.stop>
          <label :class="labelClass">Create project</label>
          <div class="flex gap-2">
            <input
              v-model="draftName"
              type="text"
              placeholder="New project name"
              class="flex-1 rounded-md border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 focus:outline-none dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100"
            />
            <button
              type="submit"
              class="rounded-md border border-neutral-300 px-3 py-2 text-sm font-semibold text-neutral-900 dark:border-neutral-600 dark:text-neutral-50"
              :disabled="isCreating"
            >
              Add
            </button>
          </div>
        </form>
      </UiMenuContent>
    </UiMenu>
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

const props = withDefaults(defineProps<{ variant?: "default" | "compact" | "mini" }>(), {
  variant: "default",
})

const projectStore = useProjectStore()
const draftName = ref("")
const isCreating = ref(false)
const menuOpen = ref(false)

const wrapperClass = computed(() => {
  if (props.variant === "compact") return "w-full"
  if (props.variant === "mini") return "max-w-[220px]"
  return "max-w-xl"
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
  const ms = new Date(value).getTime()
  if (Number.isNaN(ms)) return "--"
  return formatTsFull(ms)
}

async function select(projectId: number) {
  await projectStore.selectProject(projectId)
  menuOpen.value = false
}

async function handleCreate() {
  const trimmed = draftName.value.trim()
  if (!trimmed) return
  isCreating.value = true
  try {
    await projectStore.createProject(trimmed)
    draftName.value = ""
    menuOpen.value = false
  } finally {
    isCreating.value = false
  }
}

onMounted(() => {
  void projectStore.bootstrap()
})
</script>
