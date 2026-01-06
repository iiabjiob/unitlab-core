<template>
  <div ref="root" class="relative isolate" :class="variant === 'compact' ? 'w-full' : 'max-w-xl'">
    <button
      class="group flex w-full items-center justify-between gap-4 rounded-[28px] border border-white/20 bg-gradient-to-r from-emerald-500 via-cyan-500 to-blue-600 px-6 py-4 text-left text-white shadow-[0_20px_45px_-25px_rgba(16,185,129,0.8)] transition hover:translate-y-0.5 hover:shadow-[0_25px_55px_-25px_rgba(6,182,212,0.9)] dark:from-emerald-400 dark:via-cyan-400 dark:to-sky-500"
      :class="variant === 'compact' ? 'px-4 py-3 text-sm' : ''"
      type="button"
      :disabled="loading"
      @click="toggle"
    >
      <div>
        <p class="text-[10px] uppercase tracking-[0.4em] text-white/70">Project</p>
        <p class="text-lg font-semibold leading-tight" :class="!hasProject ? 'opacity-70 italic' : ''">
          {{ currentLabel }}
        </p>
      </div>
      <div class="flex flex-col items-end text-xs">
        <span class="text-white/80">{{ projectStore.projects.length }} available</span>
        <span
          class="font-mono text-[11px] text-white/60"
          v-if="projectStore.activeProject"
        >
          #{{ projectStore.activeProject.id }} · {{ projectStore.activeProject.uuid.slice(0, 8) }}
        </span>
      </div>
      <span
        class="inline-flex h-8 w-8 items-center justify-center rounded-full border border-white/30 bg-white/10 text-white transition group-hover:bg-white/20"
        :class="open ? 'rotate-180' : ''"
      >
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" class="h-4 w-4">
          <path stroke-linecap="round" stroke-linejoin="round" d="m6 9 6 6 6-6" />
        </svg>
      </span>
    </button>

    <transition name="fade">
      <div
        v-if="open"
        class="absolute right-0 top-[calc(100%+0.75rem)] z-30 w-[320px] rounded-2xl border border-neutral-200/40 bg-white/95 p-4 text-neutral-800 shadow-2xl backdrop-blur dark:border-neutral-700/50 dark:bg-neutral-900/95 dark:text-neutral-100"
      >
        <div class="mb-3 flex items-center justify-between text-xs uppercase tracking-[0.4em] text-neutral-500 dark:text-neutral-400">
          <span>Projects</span>
          <button
            class="rounded-full border border-neutral-200 px-3 py-1 text-[10px] font-semibold uppercase tracking-[0.3em] text-neutral-500 transition hover:border-neutral-400 hover:text-neutral-700 dark:border-neutral-700 dark:text-neutral-300 dark:hover:border-neutral-500"
            type="button"
            :disabled="loading"
            @click.stop="refresh"
          >
            Refresh
          </button>
        </div>

        <div class="max-h-56 space-y-2 overflow-y-auto pr-1">
          <button
            v-for="project in projectStore.projects"
            :key="project.id"
            class="flex w-full items-center justify-between rounded-xl border border-transparent px-3 py-2 text-left transition hover:border-neutral-200 hover:bg-neutral-50 dark:hover:border-neutral-700 dark:hover:bg-neutral-800"
            :class="project.id === projectStore.activeProjectId ? 'border-emerald-300 bg-emerald-50/70 dark:border-emerald-500/50 dark:bg-emerald-500/10' : ''"
            type="button"
            @click.stop="select(project.id)"
          >
            <div>
              <p class="text-sm font-semibold">{{ project.name }}</p>
              <p class="font-mono text-[11px] text-neutral-500 dark:text-neutral-400">{{ project.uuid }}</p>
            </div>
            <span
              v-if="project.id === projectStore.activeProjectId"
              class="rounded-full bg-emerald-500/20 px-3 py-1 text-[10px] font-semibold uppercase tracking-[0.3em] text-emerald-700 dark:text-emerald-300"
            >
              Active
            </span>
          </button>

          <div v-if="!projectStore.projects.length && !loading" class="rounded-xl border border-dashed border-neutral-200 px-3 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
            No projects yet. Create one below.
          </div>
        </div>

        <form class="mt-4 space-y-2" @submit.prevent="handleCreate">
          <label class="text-[10px] font-semibold uppercase tracking-[0.4em] text-neutral-500 dark:text-neutral-400">Create project</label>
          <div class="flex gap-2">
            <input
              v-model="draftName"
              type="text"
              placeholder="New project name"
              class="flex-1 rounded-xl border border-neutral-200 bg-white/70 px-3 py-2 text-sm text-neutral-800 outline-none transition focus:border-emerald-400 dark:border-neutral-700 dark:bg-neutral-800 dark:text-neutral-100"
            />
            <button
              type="submit"
              class="rounded-xl bg-neutral-900 px-4 py-2 text-sm font-semibold text-white transition hover:bg-neutral-800 disabled:opacity-50 dark:bg-neutral-100 dark:text-neutral-900"
              :disabled="isCreating"
            >
              Create
            </button>
          </div>
        </form>
      </div>
    </transition>
  </div>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from "vue"
import { useProjectStore } from "@/stores/projectStore"

const props = withDefaults(defineProps<{ variant?: "default" | "compact" }>(), {
  variant: "default",
})
const variant = computed(() => props.variant)

const projectStore = useProjectStore()
const open = ref(false)
const draftName = ref("")
const isCreating = ref(false)
const root = ref<HTMLElement | null>(null)

const loading = computed(() => projectStore.loading)
const hasProject = computed(() => Boolean(projectStore.activeProject))
const currentLabel = computed(() => projectStore.activeProject?.name ?? "Open a project")

async function ensureProjectsLoaded() {
  await projectStore.bootstrap()
}

function toggle() {
  open.value = !open.value
}

async function select(projectId: number) {
  await projectStore.selectProject(projectId)
  open.value = false
}

async function refresh() {
  await projectStore.refresh()
}

async function handleCreate() {
  if (!draftName.value.trim()) return
  isCreating.value = true
  try {
    await projectStore.createProject(draftName.value)
    draftName.value = ""
    open.value = false
  } finally {
    isCreating.value = false
  }
}

function handleDocumentClick(event: MouseEvent) {
  if (!open.value) return
  if (!root.value) return
  if (root.value.contains(event.target as Node)) return
  open.value = false
}

onMounted(() => {
  void ensureProjectsLoaded()
  document.addEventListener("click", handleDocumentClick)
})

onBeforeUnmount(() => {
  document.removeEventListener("click", handleDocumentClick)
})
</script>
