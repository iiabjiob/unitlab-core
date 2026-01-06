import { defineStore } from "pinia"
import { computed, ref } from "vue"

import type { Project } from "@/types/project"
import { ProjectsAPI } from "@/api/projects.api"
import { getLogger } from "@/utils/logger"

const STORAGE_KEY = "active-project-id"
const logger = getLogger("PROJECTS")

export const useProjectStore = defineStore("projectStore", () => {
  const projects = ref<Project[]>([])
  const activeProjectId = ref<number | null>(null)
  const loading = ref(false)
  const bootstrapped = ref(false)
  const error = ref<string | null>(null)

  hydrateFromStorage()

  function hydrateFromStorage() {
    if (typeof window === "undefined") return
    const raw = window.localStorage.getItem(STORAGE_KEY)
    if (!raw) return
    const parsed = Number(raw)
    if (!Number.isNaN(parsed)) {
      activeProjectId.value = parsed
    }
  }

  function persistSelection(id: number | null) {
    if (typeof window === "undefined") return
    if (id) {
      window.localStorage.setItem(STORAGE_KEY, String(id))
    } else {
      window.localStorage.removeItem(STORAGE_KEY)
    }
  }

  async function bootstrap(force = false) {
    if (bootstrapped.value && !force) return
    await fetchProjects()
    bootstrapped.value = true
  }

  async function fetchProjects() {
    loading.value = true
    error.value = null
    try {
      const { data } = await ProjectsAPI.list()
      projects.value = data
      reconcileActiveSelection()
      logger.debug(`📁 Loaded ${data.length} projects`)
    } catch (err) {
      logger.error("💥 Failed to fetch projects", err)
      error.value = err instanceof Error ? err.message : String(err)
    } finally {
      loading.value = false
    }
  }

  function reconcileActiveSelection() {
    if (!projects.value.length) {
      setActiveProject(null)
      return
    }

    if (activeProjectId.value && projects.value.some(p => p.id === activeProjectId.value)) {
      return
    }

    setActiveProject(projects.value[0].id)
  }

  function setActiveProject(projectId: number | null) {
    activeProjectId.value = projectId
    persistSelection(projectId)
    if (projectId) {
      logger.info(`🎯 Switched to project #${projectId}`)
    }
  }

  function requireProjectId(): number {
    const id = activeProjectId.value
    if (!id) {
      throw new Error("Active project is not selected.")
    }
    return id
  }

  async function selectProject(projectId: number) {
    if (projectId === activeProjectId.value) return
    setActiveProject(projectId)
  }

  async function createProject(name: string) {
    const trimmed = name.trim()
    if (!trimmed) {
      throw new Error("Project name is required")
    }

    const { data } = await ProjectsAPI.create({ name: trimmed })
    projects.value.push(data)
    setActiveProject(data.id)
    return data
  }

  async function renameProject(projectId: number, name: string) {
    const trimmed = name.trim()
    if (!trimmed) {
      throw new Error("Project name is required")
    }

    const { data } = await ProjectsAPI.update(projectId, { name: trimmed })
    const idx = projects.value.findIndex(p => p.id === projectId)
    if (idx !== -1) {
      projects.value[idx] = data
    }
    return data
  }

  async function deleteProject(projectId: number) {
    await ProjectsAPI.remove(projectId)
    projects.value = projects.value.filter(project => project.id !== projectId)
    if (activeProjectId.value === projectId) {
      setActiveProject(null)
      reconcileActiveSelection()
    }
  }

  const activeProject = computed(() =>
    projects.value.find(project => project.id === activeProjectId.value) ?? null,
  )

  const hasProjects = computed(() => projects.value.length > 0)
  const ready = computed(() => Boolean(activeProject.value))

  return {
    projects,
    activeProjectId,
    activeProject,
    hasProjects,
    ready,
    loading,
    bootstrapped,
    error,
    bootstrap,
    fetchProjects,
    refresh: fetchProjects,
    selectProject,
    setActiveProject,
    requireProjectId,
    createProject,
    renameProject,
    deleteProject,
  }
})
