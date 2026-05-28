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
      await removeMany([id])
    } catch (err) {
      params.logger.error(`💥 Failed to delete switchgear ${id}:`, err)
    }
  }

  async function removeMany(ids: number[]) {
    const workspaceId = params.requireWorkspaceId()
    const requestedIds = Array.from(new Set(ids.filter(id => Number.isFinite(id))))
    const requestedIdSet = new Set(requestedIds)
    const previous = [...params.switchgears.value]
    const removed = previous.filter(s => requestedIdSet.has(s.id))
    if (!removed.length) {
      return { deleted: 0 }
    }

    const removeIds = new Set(removed.map(s => s.id))
    params.switchgears.value = params.switchgears.value.filter(s => !removeIds.has(s.id))

    const results = await Promise.allSettled(
      removed.map(s => SwitchgearsAPI.delete(workspaceId, s.id)),
    )
    const failedIds = removed
      .filter((_, index) => results[index]?.status === "rejected")
      .map(s => s.id)

    if (failedIds.length) {
      const failedIdSet = new Set(failedIds)
      params.switchgears.value = restoreFailedSwitchgears(previous, params.switchgears.value, failedIdSet)
      const firstFailure = results.find((result): result is PromiseRejectedResult => result.status === "rejected")
      params.logger.error(`💥 Failed to delete ${failedIds.length}/${removed.length} switchgears:`, firstFailure?.reason)
      throw firstFailure?.reason ?? new Error("Failed to delete switchgears")
    }

    params.logger.info(`🗑️ Deleted ${removed.length} switchgear${removed.length === 1 ? "" : "s"}`)
    return { deleted: removed.length }
  }

  return {
    fetchAll,
    ensureLoaded,
    create,
    updateField,
    remove,
    removeMany,
  }
}

function restoreFailedSwitchgears(
  previous: Switchgear[],
  current: Switchgear[],
  failedIds: Set<number>,
): Switchgear[] {
  const currentById = new Map(current.map(item => [item.id, item]))
  const previousIds = new Set(previous.map(item => item.id))
  const restoredInOriginalOrder = previous
    .filter(item => failedIds.has(item.id) || currentById.has(item.id))
    .map(item => currentById.get(item.id) ?? item)
  const currentExtras = current.filter(item => !previousIds.has(item.id))
  return [...restoredInOriginalOrder, ...currentExtras]
}
