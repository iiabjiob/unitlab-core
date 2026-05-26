import { defineStore } from "pinia"
import { computed, ref } from "vue"

import type { Workspace } from "@/types/workspace"
import { WorkspacesAPI } from "@/api/workspaces.api"
import { localSettingsKeys, readLocalSetting, removeLocalSetting, writeLocalSetting } from "@/services/localSettingsStorage"
import { getLogger } from "@/utils/logger"

const LEGACY_STORAGE_KEY = "active-workspace-id"
const DEFAULT_WORKSPACE_NAME = "Default"
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
  const autoCreatingDefault = ref(false)
  let bootstrapInFlight: Promise<boolean> | null = null

  hydrateFromStorage()

  function hydrateFromStorage() {
    const storedWorkspaceId = readLocalSetting<number | null>(
      localSettingsKeys.activeWorkspaceId,
      null,
      {
        legacyKeys: [LEGACY_STORAGE_KEY],
        validate: normalizeWorkspaceId,
      },
    )
    if (storedWorkspaceId) {
      activeWorkspaceId.value = storedWorkspaceId
    }
  }

  function persistSelection(id: number | null) {
    if (id) {
      writeLocalSetting(localSettingsKeys.activeWorkspaceId, id, {
        legacyKeys: [LEGACY_STORAGE_KEY],
      })
    } else {
      removeLocalSetting(localSettingsKeys.activeWorkspaceId, {
        legacyKeys: [LEGACY_STORAGE_KEY],
      })
    }
  }

  async function bootstrap(force = false): Promise<boolean> {
    if (bootstrapped.value && !force) return true
    if (bootstrapInFlight && !force) {
      return await bootstrapInFlight
    }

    bootstrapInFlight = (async () => {
      const ok = await fetchWorkspaces()
      if (ok) {
        bootstrapped.value = true
      }
      return ok
    })()

    try {
      return await bootstrapInFlight
    } finally {
      bootstrapInFlight = null
    }
  }

  async function fetchWorkspaces(): Promise<boolean> {
    loading.value = true
    error.value = null
    try {
      const { data } = await WorkspacesAPI.list()
      workspaces.value = data
      logger.debug(`📁 Loaded ${data.length} workspaces`)

      if (!workspaces.value.length) {
        await ensureDefaultWorkspace()
      } else {
        reconcileActiveSelection()
      }

      return true
    } catch (err) {
      logger.error("💥 Failed to fetch workspaces", err)
      error.value = err instanceof Error ? err.message : String(err)
      return false
    } finally {
      loading.value = false
    }
  }

  async function ensureDefaultWorkspace() {
    if (autoCreatingDefault.value) return

    autoCreatingDefault.value = true
    try {
      logger.info("🆕 Provisioning default workspace")
      await createWorkspace(DEFAULT_WORKSPACE_NAME)
      logger.info("✅ Default workspace is ready")
    } catch (err) {
      logger.error("💥 Failed to auto-create default workspace", err)
      throw err
    } finally {
      autoCreatingDefault.value = false
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
    const wasActiveWorkspace = activeWorkspaceId.value === workspaceId
    if (wasActiveWorkspace) {
      setActiveWorkspace(null)
    }

    if (!workspaces.value.length) {
      await ensureDefaultWorkspace()
      return
    }

    if (wasActiveWorkspace) {
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

function normalizeWorkspaceId(value: unknown): number | null {
  const id = Number(value)
  return Number.isFinite(id) && id > 0 ? id : null
}
