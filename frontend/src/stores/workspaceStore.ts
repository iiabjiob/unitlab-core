import { defineStore } from "pinia"
import { computed, ref } from "vue"

import type { Workspace } from "@/types/workspace"
import { WorkspacesAPI } from "@/api/workspaces.api"
import { getLogger } from "@/utils/logger"

const STORAGE_KEY = "active-workspace-id"
const logger = getLogger("WORKSPACES")

function slugify(input: string): string {
  const normalized = input
    .toLowerCase()
    .trim()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
  return normalized || "workspace"
}

export const useWorkspaceStore = defineStore("workspaceStore", () => {
  const workspaces = ref<Workspace[]>([])
  const activeWorkspaceId = ref<number | null>(null)
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
      activeWorkspaceId.value = parsed
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
    await fetchWorkspaces()
    bootstrapped.value = true
  }

  async function fetchWorkspaces() {
    loading.value = true
    error.value = null
    try {
      const { data } = await WorkspacesAPI.list()
      workspaces.value = data
      reconcileActiveSelection()
      logger.debug(`📁 Loaded ${data.length} workspaces`)
    } catch (err) {
      logger.error("💥 Failed to fetch workspaces", err)
      error.value = err instanceof Error ? err.message : String(err)
    } finally {
      loading.value = false
    }
  }

  function reconcileActiveSelection() {
    if (!workspaces.value.length) {
      setActiveWorkspace(null)
      return
    }

    if (activeWorkspaceId.value && workspaces.value.some(w => w.id === activeWorkspaceId.value)) {
      return
    }

    setActiveWorkspace(workspaces.value[0].id)
  }

  function setActiveWorkspace(workspaceId: number | null) {
    activeWorkspaceId.value = workspaceId
    persistSelection(workspaceId)
    if (workspaceId) {
      logger.info(`🎯 Switched to workspace #${workspaceId}`)
    }
  }

  function requireWorkspaceId(): number {
    const id = activeWorkspaceId.value
    if (!id) {
      throw new Error("Active workspace is not selected.")
    }
    return id
  }

  async function selectWorkspace(workspaceId: number) {
    if (workspaceId === activeWorkspaceId.value) return
    setActiveWorkspace(workspaceId)
  }

  async function createWorkspace(name: string) {
    const trimmed = name.trim()
    if (!trimmed) {
      throw new Error("Workspace name is required")
    }

    const { data } = await WorkspacesAPI.create({ name: trimmed, slug: slugify(trimmed) })
    workspaces.value.push(data)
    setActiveWorkspace(data.id)
    return data
  }

  async function renameWorkspace(workspaceId: number, name: string) {
    const trimmed = name.trim()
    if (!trimmed) {
      throw new Error("Workspace name is required")
    }

    const { data } = await WorkspacesAPI.update(workspaceId, { name: trimmed })
    const idx = workspaces.value.findIndex(w => w.id === workspaceId)
    if (idx !== -1) {
      workspaces.value[idx] = data
    }
    return data
  }

  async function deleteWorkspace(workspaceId: number) {
    await WorkspacesAPI.remove(workspaceId)
    workspaces.value = workspaces.value.filter(workspace => workspace.id !== workspaceId)
    if (activeWorkspaceId.value === workspaceId) {
      setActiveWorkspace(null)
      reconcileActiveSelection()
    }
  }

  const activeWorkspace = computed(() =>
    workspaces.value.find(workspace => workspace.id === activeWorkspaceId.value) ?? null,
  )

  const hasWorkspaces = computed(() => workspaces.value.length > 0)
  const ready = computed(() => Boolean(activeWorkspace.value))

  return {
    workspaces,
    activeWorkspaceId,
    activeWorkspace,
    hasWorkspaces,
    ready,
    loading,
    bootstrapped,
    error,
    bootstrap,
    fetchWorkspaces,
    refresh: fetchWorkspaces,
    selectWorkspace,
    setActiveWorkspace,
    requireWorkspaceId,
    createWorkspace,
    renameWorkspace,
    deleteWorkspace,
  }
})
