import type { Ref } from "vue"

import { SwitchgearsAPI } from "@/api/switchgears.api"
import type { Switchgear, SwitchgearCreateInput, SwitchgearUpdateInput } from "@/types/switchgear"

type LoggerLike = {
  info: (message: string) => void
  debug: (message: string, ...args: unknown[]) => void
  error: (message: string, ...args: unknown[]) => void
}

type Params = {
  logger: LoggerLike
  switchgears: Ref<Switchgear[]>
  loading: Ref<boolean>
  loadedOnce: Ref<boolean>
  updateQueueById: Map<number, Promise<void>>
  getActiveWorkspaceId: () => number | null
  requireWorkspaceId: () => number
  buildEmptyBindings: () => Array<{ role: string; channel_id: number | null; delay_ms: number }>
}

export function createSwitchgearCatalogCrud(params: Params) {
  async function fetchAll() {
    if (!params.getActiveWorkspaceId()) return
    params.loading.value = true
    try {
      const workspaceId = params.requireWorkspaceId()
      const { data } = await SwitchgearsAPI.list(workspaceId)
      params.switchgears.value = data
      params.logger.info(`📡 Loaded ${data.length} switchgears for workspace ${workspaceId}`)
      params.loadedOnce.value = true
    } catch (err) {
      params.logger.error("💥 Failed to fetch switchgears:", err)
    } finally {
      params.loading.value = false
    }
  }

  async function ensureLoaded() {
    if (!params.getActiveWorkspaceId()) {
      params.logger.debug("⏸️ No active workspace selected, skipping switchgear load")
      return
    }

    if (!params.loadedOnce.value && !params.loading.value) {
      await fetchAll()
    }
  }

  async function create(payload: SwitchgearCreateInput) {
    try {
      const body = {
        switchgear_type: payload.switchgear_type ?? "switchgear",
        name: payload.name,
        bindings: (payload.bindings && payload.bindings.length
          ? payload.bindings
          : params.buildEmptyBindings()).map(binding => ({
          delay_ms: 0,
          ...binding,
        })),
      }
      const { data } = await SwitchgearsAPI.create(params.requireWorkspaceId(), body)
      params.switchgears.value.push(data)

      params.logger.info(`➕ Created switchgear id=${data.id}`)
      return data
    } catch (err) {
      params.logger.error("💥 Failed to create switchgear:", err)
      throw err
    }
  }

  async function updateField(id: number, changes: SwitchgearUpdateInput) {
    const previous = params.updateQueueById.get(id) ?? Promise.resolve()
    const run = previous.catch(() => undefined).then(async () => {
      try {
        const { data } = await SwitchgearsAPI.update(params.requireWorkspaceId(), id, changes)
        const idx = params.switchgears.value.findIndex(s => s.id === id)
        if (idx !== -1) {
          params.switchgears.value[idx] = data
        }
        params.logger.debug(`✏️ Switchgear ${id} updated`, changes)
        return data
      } catch (err) {
        params.logger.error(`💥 Failed to update switchgear ${id}:`, err)
        throw err
      }
    })
    params.updateQueueById.set(id, run.then(() => undefined, () => undefined))
    return await run
  }

  async function remove(id: number) {
    try {
      await SwitchgearsAPI.delete(params.requireWorkspaceId(), id)
      params.switchgears.value = params.switchgears.value.filter(s => s.id !== id)

      params.logger.info(`🗑️ Switchgear ${id} deleted`)
    } catch (err) {
      params.logger.error(`💥 Failed to delete switchgear ${id}:`, err)
    }
  }

  return {
    fetchAll,
    ensureLoaded,
    create,
    updateField,
    remove,
  }
}
