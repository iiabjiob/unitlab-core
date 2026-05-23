import { defineStore } from "pinia"
import { computed, ref } from "vue"

import { SignalSheetAPI } from "@/api/signal_sheet.api"
import type {
  SignalAllocationActionResponse,
  SignalAllocationEnsureResponse,
  SignalAllocationRow,
  SignalAllocationUpdateItem,
  SignalAutoAllocatePayload,
  SignalImportMeta,
  SignalSheet,
  SignalSheetImportPreviewResponse,
  SignalSheetPreset,
} from "@/types/signal"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { getLogger } from "@/utils/logger"
import { devPerfIncrement, devPerfMeasureStart } from "@/utils/devPerf"
import { resolveRuntimeChannelTypeForSignal } from "@/utils/signalRuntimeMapping"

const logger = getLogger("SIGNAL_SHEET")

export const useSignalSheetStore = defineStore("signalSheetStore", () => {
  const workspaceStore = useWorkspaceStore()
  const channelStore = useChannelStore()
  const deviceStore = useDeviceStore()

  const sheet = ref<SignalSheet | null>(null)
  const presets = ref<SignalSheetPreset[]>([])
  const allocationRows = ref<SignalAllocationRow[]>([])

  const loadingSheet = ref(false)
  const loadingPresets = ref(false)
  const loadingAllocations = ref(false)
  const allocationMutationsInFlight = ref(0)
  const importing = ref(false)
  const lastSheetLoadedAt = ref<number | null>(null)
  const lastPresetsLoadedAt = ref<number | null>(null)
  const lastAllocationsLoadedAt = ref<number | null>(null)
  const allocationRevision = ref(0)
  const recentlyChangedSignalIds = ref<number[]>([])

  const initializedWorkspaceId = ref<number | null>(null)
  let sheetInFlight: Promise<SignalSheet | null> | null = null
  let sheetInFlightWorkspaceId: number | null = null
  let allocationsInFlight: Promise<SignalAllocationRow[]> | null = null
  let allocationsInFlightWorkspaceId: number | null = null
  const allocationIndexBySignalId = new Map<number, number>()
  const allocationOwnerByChannelId = new Map<number, number>()
  const allocationMutationVersionBySignalId = new Map<number, number>()
  let allocationMutationVersionCounter = 0
  const ALLOCATION_BATCH_SIZE = 200
  const ALLOCATION_REFRESH_PAGE_SIZE = 400
  const ALLOCATION_REFRESH_MIN_PAGE_SIZE = 50
  const TESTED_AT_PATCH_FLUSH_MS = 160
  const TESTED_AT_PATCH_CHUNK_SIZE = 80
  const DEFAULT_SHEET_TTL_MS = 1_500
  const DEFAULT_ALLOCATIONS_TTL_MS = 1_500
  const DEFAULT_PRESETS_TTL_MS = 10_000
  const pendingTestedAtPatchBySignalId = new Map<number, string>()
  let testedAtPatchFlushTimer: ReturnType<typeof setTimeout> | null = null
  let testedAtPatchFlushFrame: number | null = null

  function requireWorkspaceId(): number {
    return workspaceStore.requireWorkspaceId()
  }

  function isCacheFresh(lastLoadedAt: number | null, ttlMs: number): boolean {
    if (!Number.isFinite(lastLoadedAt as number)) {
      return false
    }
    return (Date.now() - Number(lastLoadedAt)) < Math.max(0, ttlMs)
  }

  function bumpAllocationRevision() {
    allocationRevision.value += 1
  }

  function setRecentlyChangedSignalIds(signalIds: readonly number[]) {
    if (!signalIds.length) {
      recentlyChangedSignalIds.value = []
      return
    }
    recentlyChangedSignalIds.value = [...new Set(signalIds.map(item => Number(item)).filter(Number.isFinite))]
  }

  function isFiniteChannelId(value: unknown): value is number {
    return typeof value === "number" && Number.isFinite(value)
  }

  function isAllocatedChannelId(value: unknown): value is number {
    return typeof value === "number" && Number.isInteger(value) && value > 0
  }

  function normalizeChannelId(value: unknown): number | null {
    return isAllocatedChannelId(value) ? value : null
  }

  function cloneAllocationRow(row: SignalAllocationRow): SignalAllocationRow {
    return {
      ...row,
      signal_metadata: row.signal_metadata && typeof row.signal_metadata === "object"
        ? { ...row.signal_metadata }
        : {},
    }
  }

  function beginAllocationMutation() {
    allocationMutationsInFlight.value += 1
  }

  function endAllocationMutation() {
    allocationMutationsInFlight.value = Math.max(0, allocationMutationsInFlight.value - 1)
  }

  function isRetryableAllocationPageError(error: unknown): boolean {
    if (!(error instanceof Error)) {
      return false
    }
    const message = String(error.message ?? "")
    return /content_length_mismatch|content-length|length\s*mismatch|err_network|econnreset|etimedout/i.test(message)
  }

  async function fetchAllocationPageAdaptive(
    workspaceId: number,
    offset: number,
    pageSize: number,
  ): Promise<{ rows: SignalAllocationRow[]; nextPageSize: number }> {
    let currentLimit = Math.max(ALLOCATION_REFRESH_MIN_PAGE_SIZE, Math.floor(pageSize))

    while (true) {
      try {
        const { data } = await SignalSheetAPI.listAllocations(workspaceId, {
          offset,
          limit: currentLimit,
        })
        const rows = Array.isArray(data) ? data : []
        return { rows, nextPageSize: currentLimit }
      } catch (error) {
        if (!isRetryableAllocationPageError(error) || currentLimit <= ALLOCATION_REFRESH_MIN_PAGE_SIZE) {
          throw error
        }
        currentLimit = Math.max(ALLOCATION_REFRESH_MIN_PAGE_SIZE, Math.floor(currentLimit / 2))
      }
    }
  }

  async function yieldToEventLoop() {
    await new Promise<void>((resolve) => {
      setTimeout(resolve, 0)
    })
  }

  function normalizedChannelType(value: unknown): "di" | "do" | "ai" | "ao" | null {
    const normalized = String(value ?? "").trim().toLowerCase()
    if (normalized.startsWith("di")) return "di"
    if (normalized.startsWith("do")) return "do"
    if (normalized.startsWith("ai")) return "ai"
    if (normalized.startsWith("ao")) return "ao"
    return null
  }

  function resolveAutoAllocateAssignments(payload: SignalAutoAllocatePayload): Array<{ signalId: number; channelId: number }> {
    const hasExplicitTargets = Array.isArray(payload.signal_ids)
    const targetSignalIds = hasExplicitTargets
      ? Array.from(new Set((payload.signal_ids ?? []).map(item => Number(item)).filter(id => Number.isFinite(id) && id > 0)))
      : allocationRows.value.map(row => row.signal_id)
    if (!targetSignalIds.length) {
      return []
    }

    const preferOnline = payload.prefer_online ?? true
    const overwriteExisting = payload.overwrite_existing ?? false

    const targetRows: SignalAllocationRow[] = []
    targetSignalIds.forEach((signalId) => {
      const rowIndex = allocationIndexBySignalId.get(signalId)
      if (rowIndex === undefined) return
      const row = allocationRows.value[rowIndex]
      if (!row) return
      targetRows.push(row)
    })
    if (!targetRows.length) {
      return []
    }

    const usedChannelIds = new Set<number>()
    allocationRows.value.forEach((row) => {
      const channelId = normalizeChannelId(row.channel_id)
      if (channelId !== null) {
        usedChannelIds.add(channelId)
      }
    })

    if (overwriteExisting) {
      targetRows.forEach((row) => {
        const channelId = normalizeChannelId(row.channel_id)
        if (channelId !== null) {
          usedChannelIds.delete(channelId)
        }
      })
    }

    const deviceOnlineById = new Map<number, boolean>()
    if (preferOnline) {
      deviceStore.devices.forEach((device) => {
        deviceOnlineById.set(device.id, device.status === "online")
      })
    }

    const sortedChannels = [...channelStore.channels].sort((left, right) => {
      const onlineLeft = preferOnline && deviceOnlineById.get(left.device_id) ? 0 : 1
      const onlineRight = preferOnline && deviceOnlineById.get(right.device_id) ? 0 : 1
      if (onlineLeft !== onlineRight) {
        return onlineLeft - onlineRight
      }
      if (left.device_id !== right.device_id) {
        return left.device_id - right.device_id
      }
      if (left.index !== right.index) {
        return left.index - right.index
      }
      return left.id - right.id
    })

    const channelsByType: Record<"di" | "do" | "ai" | "ao", typeof sortedChannels> = {
      di: [],
      do: [],
      ai: [],
      ao: [],
    }
    sortedChannels.forEach((channel) => {
      const type = normalizedChannelType(channel.type)
      if (!type) return
      channelsByType[type].push(channel)
    })

    const nextChannelIndexByType: Record<"di" | "do" | "ai" | "ao", number> = {
      di: 0,
      do: 0,
      ai: 0,
      ao: 0,
    }
    const assignments: Array<{ signalId: number; channelId: number }> = []
    targetRows.forEach((row) => {
      const existingChannelId = normalizeChannelId(row.channel_id)
      if (existingChannelId !== null && !overwriteExisting) {
        return
      }

      const requiredType = resolveRuntimeChannelTypeForSignal(String(row.signal_direction ?? ""))
      if (!requiredType) {
        return
      }

      const candidates = channelsByType[requiredType]
      let cursor = nextChannelIndexByType[requiredType]
      while (cursor < candidates.length && usedChannelIds.has(candidates[cursor].id)) {
        cursor += 1
      }
      nextChannelIndexByType[requiredType] = cursor
      const candidate = cursor < candidates.length ? candidates[cursor] : null
      if (!candidate) {
        return
      }

      if (existingChannelId === candidate.id) {
        usedChannelIds.add(candidate.id)
        nextChannelIndexByType[requiredType] = cursor + 1
        return
      }

      assignments.push({
        signalId: row.signal_id,
        channelId: candidate.id,
      })
      usedChannelIds.add(candidate.id)
      nextChannelIndexByType[requiredType] = cursor + 1
    })

    return assignments
  }

  function rebuildAllocationIndexes() {
    allocationIndexBySignalId.clear()
    allocationOwnerByChannelId.clear()
    allocationRows.value.forEach((row, index) => {
      allocationIndexBySignalId.set(row.signal_id, index)
      const channelId = normalizeChannelId(row.channel_id)
      if (channelId !== null) {
        allocationOwnerByChannelId.set(channelId, row.signal_id)
      }
    })
  }

  function replaceAllocationRows(
    rows: SignalAllocationRow[],
    changedSignalIds: readonly number[] = [],
    options?: { skipRecentlyChanged?: boolean; skipRevision?: boolean },
  ) {
    allocationRows.value = rows
    rebuildAllocationIndexes()
    if (!options?.skipRecentlyChanged) {
      setRecentlyChangedSignalIds(changedSignalIds)
    }
    if (!options?.skipRevision) {
      bumpAllocationRevision()
    }
  }

  function recomputeSheetAllocatedCount() {
    if (!sheet.value) return
    const next = allocationRows.value.reduce((count, row) => (
      count + (isAllocatedChannelId(row.channel_id) ? 1 : 0)
    ), 0)
    if (sheet.value.allocated_count === next) {
      return
    }
    sheet.value = {
      ...sheet.value,
      allocated_count: next,
    }
  }

  function patchSheetAllocatedCount(previousAllocated: boolean, nextAllocated: boolean) {
    if (!sheet.value || previousAllocated === nextAllocated) {
      return
    }
    const delta = (nextAllocated ? 1 : 0) - (previousAllocated ? 1 : 0)
    sheet.value = {
      ...sheet.value,
      allocated_count: Math.max(0, sheet.value.allocated_count + delta),
    }
  }

  function applyLocalTestedAtPatch(signalIds: readonly number[], testedAtIso: string) {
    if (!signalIds.length) {
      return
    }
    signalIds.forEach((signalId) => {
      const rowIndex = allocationIndexBySignalId.get(signalId)
      if (rowIndex === undefined) return
      const row = allocationRows.value[rowIndex]
      if (!row) return
      row.tested_at = testedAtIso
    })
  }

  function applyTestedAtBySignalPatch(testedAtBySignal: Record<number, string> | Record<string, string>) {
    const entries = Object.entries(testedAtBySignal ?? {})
    if (!entries.length) {
      return
    }

    entries.forEach(([rawSignalId, testedAtIso]) => {
      const signalId = Number(rawSignalId)
      if (!Number.isFinite(signalId) || signalId <= 0) {
        return
      }
      const testedAt = String(testedAtIso ?? "").trim()
      if (!testedAt) {
        return
      }
      pendingTestedAtPatchBySignalId.set(signalId, testedAt)
    })

    if (pendingTestedAtPatchBySignalId.size === 0) {
      return
    }

    if (testedAtPatchFlushTimer !== null || testedAtPatchFlushFrame !== null) {
      return
    }

    testedAtPatchFlushTimer = setTimeout(() => {
      testedAtPatchFlushTimer = null
      flushTestedAtPatchChunk()
    }, TESTED_AT_PATCH_FLUSH_MS)
  }

  function scheduleTestedAtPatchFrameFlush() {
    if (pendingTestedAtPatchBySignalId.size === 0) {
      return
    }
    if (testedAtPatchFlushFrame !== null || testedAtPatchFlushTimer !== null) {
      return
    }
    if (typeof requestAnimationFrame === "function") {
      testedAtPatchFlushFrame = requestAnimationFrame(() => {
        testedAtPatchFlushFrame = null
        flushTestedAtPatchChunk()
      })
      return
    }
    testedAtPatchFlushTimer = setTimeout(() => {
      testedAtPatchFlushTimer = null
      flushTestedAtPatchChunk()
    }, 16)
  }

  function flushTestedAtPatchChunk() {
    if (pendingTestedAtPatchBySignalId.size === 0) {
      return
    }

    let processed = 0
    for (const [signalId, testedAt] of pendingTestedAtPatchBySignalId.entries()) {
      const rowIndex = allocationIndexBySignalId.get(signalId)
      pendingTestedAtPatchBySignalId.delete(signalId)
      if (rowIndex === undefined) {
        processed += 1
        if (processed >= TESTED_AT_PATCH_CHUNK_SIZE) {
          break
        }
        continue
      }
      const row = allocationRows.value[rowIndex]
      if (row && row.tested_at !== testedAt) {
        row.tested_at = testedAt
      }

      processed += 1
      if (processed >= TESTED_AT_PATCH_CHUNK_SIZE) {
        break
      }
    }

    if (pendingTestedAtPatchBySignalId.size > 0) {
      scheduleTestedAtPatchFrameFlush()
    }
  }

  function resolveChannelLabel(unitId: string | null, channelIndex: number | null): string | null {
    if (!Number.isFinite(channelIndex as number)) {
      return null
    }
    const suffix = `ch${Number(channelIndex) + 1}`
    return unitId ? `${unitId}/${suffix}` : suffix
  }

  function applyLocalAllocationPatch(
    signalId: number,
    nextChannelIdRaw: number | null | undefined,
    options?: { skipRevision?: boolean },
  ) {
    const rowIndex = allocationIndexBySignalId.get(signalId)
    if (rowIndex === undefined) {
      return
    }
    const currentRow = allocationRows.value[rowIndex]
    if (!currentRow) {
      return
    }

    const previousAllocated = isAllocatedChannelId(currentRow.channel_id)
    const previousChannelId = normalizeChannelId(currentRow.channel_id)
    const nextChannelId = normalizeChannelId(nextChannelIdRaw)

    if (nextChannelId !== null) {
      const existingOwnerSignalId = allocationOwnerByChannelId.get(nextChannelId)
      if (existingOwnerSignalId !== undefined && existingOwnerSignalId !== signalId) {
        return
      }
    }

    const nextRow: SignalAllocationRow = { ...currentRow }

    if (nextChannelId === null) {
      nextRow.channel_id = null
      nextRow.channel_type = null
      nextRow.channel_index = null
      nextRow.channel_label = null
      nextRow.device_id = null
      nextRow.unit_id = null
      nextRow.unit_online = null
      nextRow.unit_last_seen_at = null
    } else {
      const channel = channelStore.channels.find(item => item.id === nextChannelId)
      nextRow.channel_id = nextChannelId
      if (!channel) {
        nextRow.channel_type = null
        nextRow.channel_index = null
        nextRow.channel_label = null
        nextRow.device_id = null
        nextRow.unit_id = null
        nextRow.unit_online = null
        nextRow.unit_last_seen_at = null
      } else {
        const device = deviceStore.devices.find(item => item.id === channel.device_id)
        const unitId = device?.unit_id ?? channelStore.resolveUnitId(channel.device_id) ?? null
        nextRow.channel_type = channel.type
        nextRow.channel_index = channel.index
        nextRow.device_id = channel.device_id
        nextRow.unit_id = unitId
        nextRow.channel_label = resolveChannelLabel(unitId, channel.index)
        nextRow.unit_online = device ? device.status === "online" : null
        nextRow.unit_last_seen_at = null
      }
    }

    allocationRows.value[rowIndex] = nextRow
    if (previousChannelId !== null && allocationOwnerByChannelId.get(previousChannelId) === signalId) {
      allocationOwnerByChannelId.delete(previousChannelId)
    }
    if (nextChannelId !== null) {
      allocationOwnerByChannelId.set(nextChannelId, signalId)
    }

    const nextAllocated = isAllocatedChannelId(nextRow.channel_id)
    patchSheetAllocatedCount(previousAllocated, nextAllocated)
    if (!options?.skipRevision) {
      bumpAllocationRevision()
    }
  }

  function applyServerAllocationPatch(
    serverRows: SignalAllocationRow[],
    signalIds: readonly number[],
    options?: { skipRecentlyChanged?: boolean; skipRevision?: boolean },
  ) {
    devPerfIncrement("signalSheet.applyServerAllocationPatch.calls")
    const endMeasure = devPerfMeasureStart("signalSheet.applyServerAllocationPatch")
    if (!signalIds.length) {
      endMeasure({ skipped: true, reason: "empty-signal-ids" })
      return
    }
    const changedSet = new Set(signalIds)
    const serverBySignalId = new Map<number, SignalAllocationRow>()
    serverRows.forEach((row) => {
      if (changedSet.has(row.signal_id)) {
        serverBySignalId.set(row.signal_id, row)
      }
    })

    const canPatchInPlace = signalIds.every((signalId) => (
      allocationIndexBySignalId.has(signalId) && serverBySignalId.has(signalId)
    ))
    if (!canPatchInPlace) {
      devPerfIncrement("signalSheet.applyServerAllocationPatch.replaceRowsPath")
      replaceAllocationRows(serverRows, signalIds, {
        skipRecentlyChanged: options?.skipRecentlyChanged,
        skipRevision: options?.skipRevision,
      })
      recomputeSheetAllocatedCount()
      endMeasure({ mode: "replaceRows", count: signalIds.length })
      return
    }

    signalIds.forEach((signalId) => {
      const rowIndex = allocationIndexBySignalId.get(signalId)
      const serverRow = serverBySignalId.get(signalId)
      if (rowIndex === undefined || !serverRow) return
      const targetRow = allocationRows.value[rowIndex]
      if (!targetRow) return
      const previousAllocated = isAllocatedChannelId(targetRow.channel_id)
      const previousChannelId = normalizeChannelId(targetRow.channel_id)
      const nextRow = cloneAllocationRow(serverRow)
      allocationRows.value[rowIndex] = nextRow
      if (previousChannelId !== null && allocationOwnerByChannelId.get(previousChannelId) === signalId) {
        allocationOwnerByChannelId.delete(previousChannelId)
      }
      const nextChannelId = normalizeChannelId(nextRow.channel_id)
      if (nextChannelId !== null) {
        allocationOwnerByChannelId.set(nextChannelId, signalId)
      }
      const nextAllocated = isAllocatedChannelId(nextRow.channel_id)
      patchSheetAllocatedCount(previousAllocated, nextAllocated)
    })
    if (!options?.skipRecentlyChanged) {
      setRecentlyChangedSignalIds(signalIds)
    }
    if (!options?.skipRevision) {
      bumpAllocationRevision()
    }
    devPerfIncrement("signalSheet.applyServerAllocationPatch.inPlacePath")
    endMeasure({ mode: "inPlace", count: signalIds.length })
  }

  function resetState() {
    sheet.value = null
    presets.value = []
    allocationRows.value = []
    lastSheetLoadedAt.value = null
    lastPresetsLoadedAt.value = null
    lastAllocationsLoadedAt.value = null
    allocationIndexBySignalId.clear()
    allocationOwnerByChannelId.clear()
    allocationMutationVersionBySignalId.clear()
    pendingTestedAtPatchBySignalId.clear()
    if (testedAtPatchFlushTimer !== null) {
      clearTimeout(testedAtPatchFlushTimer)
      testedAtPatchFlushTimer = null
    }
    if (testedAtPatchFlushFrame !== null) {
      cancelAnimationFrame(testedAtPatchFlushFrame)
      testedAtPatchFlushFrame = null
    }
    setRecentlyChangedSignalIds([])
    initializedWorkspaceId.value = null
    sheetInFlight = null
    sheetInFlightWorkspaceId = null
    allocationsInFlight = null
    allocationsInFlightWorkspaceId = null
  }

  async function bootstrap(force = false) {
    const workspaceId = requireWorkspaceId()
    if (!force && initializedWorkspaceId.value === workspaceId) {
      return
    }
    await Promise.all([
      refreshSheet(),
      refreshPresets(),
      refreshAllocations(),
    ])
    initializedWorkspaceId.value = workspaceId
  }

  async function refreshSheet() {
    devPerfIncrement("signalSheet.refreshSheet.calls")
    const workspaceId = requireWorkspaceId()
    if (sheetInFlight && sheetInFlightWorkspaceId === workspaceId) {
      devPerfIncrement("signalSheet.refreshSheet.dedupe_waits")
      return sheetInFlight
    }

    const task = (async () => {
      const endMeasure = devPerfMeasureStart("signalSheet.refreshSheet")
      loadingSheet.value = true
      try {
        const { data } = await SignalSheetAPI.get(workspaceId)
        if (workspaceStore.activeWorkspaceId !== workspaceId) {
          endMeasure({ workspaceId, stale: true })
          return sheet.value
        }
        sheet.value = data
        lastSheetLoadedAt.value = Date.now()
        devPerfIncrement("signalSheet.refreshSheet.completed")
        endMeasure({ workspaceId, hasSheet: Boolean(data) })
        return data
      } finally {
        loadingSheet.value = false
      }
    })()

    sheetInFlight = task
    sheetInFlightWorkspaceId = workspaceId
    try {
      return await task
    } finally {
      if (sheetInFlight === task) {
        sheetInFlight = null
        sheetInFlightWorkspaceId = null
      }
    }
  }

  async function refreshPresets() {
    devPerfIncrement("signalSheet.refreshPresets.calls")
    const workspaceId = requireWorkspaceId()
    const endMeasure = devPerfMeasureStart("signalSheet.refreshPresets")
    loadingPresets.value = true
    try {
      const { data } = await SignalSheetAPI.listPresets(workspaceId)
      if (workspaceStore.activeWorkspaceId !== workspaceId) {
        endMeasure({ workspaceId, stale: true })
        return presets.value
      }
      presets.value = data
      lastPresetsLoadedAt.value = Date.now()
      devPerfIncrement("signalSheet.refreshPresets.completed")
      endMeasure({ workspaceId, count: data.length })
      return data
    } finally {
      loadingPresets.value = false
    }
  }

  async function ensureSheetLoaded(options?: { force?: boolean; ttlMs?: number }) {
    devPerfIncrement("signalSheet.ensureSheetLoaded.calls")
    const workspaceId = requireWorkspaceId()
    const force = options?.force ?? false
    const ttlMs = options?.ttlMs ?? DEFAULT_SHEET_TTL_MS
    const activeSheet = sheet.value

    if (
      !force
      && activeSheet
      && activeSheet.workspace_id === workspaceId
      && isCacheFresh(lastSheetLoadedAt.value, ttlMs)
    ) {
      devPerfIncrement("signalSheet.ensureSheetLoaded.cache_hits")
      return activeSheet
    }

    devPerfIncrement("signalSheet.ensureSheetLoaded.refreshes")
    return refreshSheet()
  }

  async function ensurePresetsLoaded(options?: { force?: boolean; ttlMs?: number }) {
    devPerfIncrement("signalSheet.ensurePresetsLoaded.calls")
    const force = options?.force ?? false
    const ttlMs = options?.ttlMs ?? DEFAULT_PRESETS_TTL_MS

    if (!force && presets.value.length > 0 && isCacheFresh(lastPresetsLoadedAt.value, ttlMs)) {
      devPerfIncrement("signalSheet.ensurePresetsLoaded.cache_hits")
      return presets.value
    }

    devPerfIncrement("signalSheet.ensurePresetsLoaded.refreshes")
    return refreshPresets()
  }

  async function refreshAllocations() {
    devPerfIncrement("signalSheet.refreshAllocations.calls")
    const workspaceId = requireWorkspaceId()
    if (allocationsInFlight && allocationsInFlightWorkspaceId === workspaceId) {
      devPerfIncrement("signalSheet.refreshAllocations.dedupe_waits")
      return allocationsInFlight
    }

    const task = (async () => {
      const endMeasure = devPerfMeasureStart("signalSheet.refreshAllocations")
      loadingAllocations.value = true
      try {
        let rows: SignalAllocationRow[] | null = null
        let usedStream = false

        try {
          rows = await SignalSheetAPI.streamAllocations(workspaceId)
          usedStream = rows !== null
        } catch {
          rows = null
        }

        if (rows === null) {
          rows = []
          let offset = 0
          let pageSize = ALLOCATION_REFRESH_PAGE_SIZE
          while (true) {
            const { rows: pageRows, nextPageSize } = await fetchAllocationPageAdaptive(workspaceId, offset, pageSize)
            pageSize = nextPageSize
            if (workspaceStore.activeWorkspaceId !== workspaceId) {
              endMeasure({ workspaceId, stale: true, mode: "paged" })
              return allocationRows.value
            }
            if (pageRows.length === 0) {
              break
            }
            rows.push(...pageRows)
            if (pageRows.length < pageSize) {
              break
            }
            offset += pageRows.length
          }
        }

        if (workspaceStore.activeWorkspaceId !== workspaceId) {
          endMeasure({ workspaceId, stale: true, mode: usedStream ? "stream" : "paged" })
          return allocationRows.value
        }
        replaceAllocationRows(rows)
        recomputeSheetAllocatedCount()
        lastAllocationsLoadedAt.value = Date.now()
        devPerfIncrement("signalSheet.refreshAllocations.completed")
        endMeasure({
          workspaceId,
          count: rows.length,
          mode: usedStream ? "stream" : "paged",
        })
        return rows
      } finally {
        loadingAllocations.value = false
      }
    })()

    allocationsInFlight = task
    allocationsInFlightWorkspaceId = workspaceId
    try {
      return await task
    } finally {
      if (allocationsInFlight === task) {
        allocationsInFlight = null
        allocationsInFlightWorkspaceId = null
      }
    }
  }

  async function ensureAllocationsLoaded(options?: { force?: boolean; ttlMs?: number }) {
    devPerfIncrement("signalSheet.ensureAllocationsLoaded.calls")
    const workspaceId = requireWorkspaceId()
    const force = options?.force ?? false
    const ttlMs = options?.ttlMs ?? DEFAULT_ALLOCATIONS_TTL_MS
    const activeSheetWorkspaceId = sheet.value?.workspace_id ?? null

    if (
      !force
      && activeSheetWorkspaceId === workspaceId
      && isCacheFresh(lastAllocationsLoadedAt.value, ttlMs)
    ) {
      devPerfIncrement("signalSheet.ensureAllocationsLoaded.cache_hits")
      return allocationRows.value
    }

    devPerfIncrement("signalSheet.ensureAllocationsLoaded.refreshes")
    return refreshAllocations()
  }

  async function importSheet(file: File, options?: {
    metadata?: SignalImportMeta | null
    presetId?: number | null
    savePresetName?: string | null
  }) {
    const workspaceId = requireWorkspaceId()
    importing.value = true
    try {
      const { data } = await SignalSheetAPI.import(workspaceId, file, options)
      sheet.value = data.sheet
      lastSheetLoadedAt.value = Date.now()
      await Promise.all([refreshAllocations(), refreshPresets()])
      logger.info("Imported signal sheet", {
        rows: data.sheet.rows_count,
        signals: data.sheet.signals_count,
      })
      return data.sheet
    } finally {
      importing.value = false
    }
  }

  async function previewImportSheet(file: File, options?: {
    metadata?: SignalImportMeta | null
    presetId?: number | null
  }): Promise<SignalSheetImportPreviewResponse> {
    const workspaceId = requireWorkspaceId()
    const { data } = await SignalSheetAPI.previewImport(workspaceId, file, options)
    return data
  }

  async function savePreset(payload: { name: string; import_meta: SignalImportMeta }) {
    const workspaceId = requireWorkspaceId()
    const { data } = await SignalSheetAPI.savePreset(workspaceId, payload)
    const idx = presets.value.findIndex(item => item.id === data.id)
    if (idx === -1) {
      presets.value = [data, ...presets.value]
    } else {
      presets.value[idx] = data
    }
    return data
  }

  async function deletePreset(presetId: number) {
    await SignalSheetAPI.deletePreset(presetId)
    presets.value = presets.value.filter(item => item.id !== presetId)
  }

  async function bulkSetAllocations(entries: SignalAllocationUpdateItem[]) {
    devPerfIncrement("signalSheet.bulkSetAllocations.calls")
    const endMeasure = devPerfMeasureStart("signalSheet.bulkSetAllocations")
    if (!entries.length) {
      endMeasure({ skipped: true, reason: "empty-entries" })
      return allocationRows.value
    }

    const deduped = new Map<number, number | null>()
    entries.forEach((entry) => {
      const normalizedChannelId = normalizeChannelId(entry.channel_id)
      deduped.set(entry.signal_id, normalizedChannelId)
    })

    const normalizedEntries = Array.from(deduped.entries()).map(([signal_id, channel_id]) => ({
      signal_id,
      channel_id,
    }))
    const effectiveEntries = normalizedEntries.filter((entry) => {
      const rowIndex = allocationIndexBySignalId.get(entry.signal_id)
      if (rowIndex === undefined) return true
      const row = allocationRows.value[rowIndex]
      if (!row) return true
      const current = normalizeChannelId(row.channel_id)
      return current !== entry.channel_id
    })
    if (!effectiveEntries.length) {
      devPerfIncrement("signalSheet.bulkSetAllocations.noop")
      endMeasure({ skipped: true, reason: "no-effective-entries", requested: entries.length })
      return allocationRows.value
    }

    const affectedSignalVersions = new Map<number, number>()
    const rollback = new Map<number, SignalAllocationRow>()

    beginAllocationMutation()
    try {
      const affectedSignalIds = effectiveEntries.map(item => item.signal_id)
      for (let index = 0; index < affectedSignalIds.length; index += 1) {
        const signalId = affectedSignalIds[index]
        allocationMutationVersionCounter += 1
        const nextVersion = allocationMutationVersionCounter
        allocationMutationVersionBySignalId.set(signalId, nextVersion)
        affectedSignalVersions.set(signalId, nextVersion)
        if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
          await yieldToEventLoop()
        }
      }

      for (let index = 0; index < affectedSignalIds.length; index += 1) {
        const signalId = affectedSignalIds[index]
        const rowIndex = allocationIndexBySignalId.get(signalId)
        if (rowIndex !== undefined) {
          const row = allocationRows.value[rowIndex]
          if (row) {
            rollback.set(signalId, cloneAllocationRow(row))
          }
        }
        if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
          await yieldToEventLoop()
        }
      }

      for (let index = 0; index < effectiveEntries.length; index += 1) {
        const entry = effectiveEntries[index]
        applyLocalAllocationPatch(entry.signal_id, entry.channel_id, { skipRevision: true })
        if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
          await yieldToEventLoop()
        }
      }
      setRecentlyChangedSignalIds(affectedSignalIds)
      bumpAllocationRevision()

      const workspaceId = requireWorkspaceId()
      const endApiMeasure = devPerfMeasureStart("signalSheet.bulkSetAllocations.api")
      const { data } = await SignalSheetAPI.updateAllocations(workspaceId, effectiveEntries)
      endApiMeasure({ workspaceId, count: effectiveEntries.length })
      const stillCurrentSignalIds = affectedSignalIds.filter((signalId) => (
        allocationMutationVersionBySignalId.get(signalId) === affectedSignalVersions.get(signalId)
      ))
      if (stillCurrentSignalIds.length > 0) {
        applyServerAllocationPatch(data, stillCurrentSignalIds)
      }
      recomputeSheetAllocatedCount()
      devPerfIncrement("signalSheet.bulkSetAllocations.completed")
      endMeasure({
        ok: true,
        requested: entries.length,
        effective: effectiveEntries.length,
        affected: affectedSignalIds.length,
      })
      return allocationRows.value
    } catch (error) {
      const rollbackEntries = Array.from(affectedSignalVersions.keys())
      for (let index = 0; index < rollbackEntries.length; index += 1) {
        const signalId = rollbackEntries[index]
        if (allocationMutationVersionBySignalId.get(signalId) !== affectedSignalVersions.get(signalId)) {
          if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
            await yieldToEventLoop()
          }
          continue
        }
        const rowIndex = allocationIndexBySignalId.get(signalId)
        const rollbackRow = rollback.get(signalId)
        if (rowIndex === undefined || !rollbackRow) {
          if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
            await yieldToEventLoop()
          }
          continue
        }
        const row = allocationRows.value[rowIndex]
        if (!row) {
          if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
            await yieldToEventLoop()
          }
          continue
        }
        const previousAllocated = isAllocatedChannelId(row.channel_id)
        const previousChannelId = normalizeChannelId(row.channel_id)
        allocationRows.value[rowIndex] = cloneAllocationRow(rollbackRow)
        if (previousChannelId !== null && allocationOwnerByChannelId.get(previousChannelId) === signalId) {
          allocationOwnerByChannelId.delete(previousChannelId)
        }
        const nextChannelId = normalizeChannelId(rollbackRow.channel_id)
        if (nextChannelId !== null) {
          allocationOwnerByChannelId.set(nextChannelId, signalId)
        }
        const nextAllocated = isAllocatedChannelId(rollbackRow.channel_id)
        patchSheetAllocatedCount(previousAllocated, nextAllocated)
        if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
          await yieldToEventLoop()
        }
      }
      recomputeSheetAllocatedCount()
      bumpAllocationRevision()
      endMeasure({
        ok: false,
        requested: entries.length,
        effective: effectiveEntries.length,
      })
      throw error
    } finally {
      endAllocationMutation()
    }
  }

  async function setAllocation(signalId: number, channelId: number | null) {
    const normalizedSignalId = Number(signalId)
    if (!Number.isFinite(normalizedSignalId) || normalizedSignalId <= 0) {
      throw new Error("Signal id is required")
    }

    const normalizedChannelId = normalizeChannelId(channelId)
    if (normalizedChannelId === null) {
      return unassignAllocation(normalizedSignalId)
    }

    const rowIndex = allocationIndexBySignalId.get(normalizedSignalId)
    const currentRow = rowIndex === undefined ? null : allocationRows.value[rowIndex] ?? null
    const currentChannelId = normalizeChannelId(currentRow?.channel_id)
    if (currentChannelId === null) {
      return assignAllocation(normalizedSignalId, normalizedChannelId)
    }
    if (currentChannelId === normalizedChannelId) {
      return {
        workspace_id: requireWorkspaceId(),
        changed_rows: currentRow ? [cloneAllocationRow(currentRow)] : [],
        conflicts: [],
        rejected: [],
      } satisfies SignalAllocationActionResponse
    }
    return reassignAllocation(normalizedSignalId, normalizedChannelId)
  }

  function applyAllocationActionResponse(data: SignalAllocationActionResponse) {
    const changedRows = Array.isArray(data.changed_rows) ? data.changed_rows : []
    if (changedRows.length > 0) {
      applyServerAllocationPatch(
        changedRows,
        changedRows.map(row => row.signal_id),
      )
      recomputeSheetAllocatedCount()
    }
    return data
  }

  async function assignAllocation(
    signalId: number,
    channelId: number,
    allocationMeta?: Record<string, unknown> | null,
  ): Promise<SignalAllocationActionResponse> {
    beginAllocationMutation()
    try {
      const workspaceId = requireWorkspaceId()
      const { data } = await SignalSheetAPI.assignAllocation(workspaceId, {
        signal_id: signalId,
        channel_id: channelId,
        allocation_meta: allocationMeta ?? null,
      })
      return applyAllocationActionResponse(data)
    } finally {
      endAllocationMutation()
    }
  }

  async function reassignAllocation(
    signalId: number,
    channelId: number,
    allocationMeta?: Record<string, unknown> | null,
  ): Promise<SignalAllocationActionResponse> {
    beginAllocationMutation()
    try {
      const workspaceId = requireWorkspaceId()
      const { data } = await SignalSheetAPI.reassignAllocation(workspaceId, {
        signal_id: signalId,
        channel_id: channelId,
        allocation_meta: allocationMeta ?? null,
      })
      return applyAllocationActionResponse(data)
    } finally {
      endAllocationMutation()
    }
  }

  async function unassignAllocation(signalId: number): Promise<SignalAllocationActionResponse> {
    beginAllocationMutation()
    try {
      const workspaceId = requireWorkspaceId()
      const { data } = await SignalSheetAPI.unassignAllocation(workspaceId, {
        signal_id: signalId,
      })
      return applyAllocationActionResponse(data)
    } finally {
      endAllocationMutation()
    }
  }

  async function swapAllocations(signalId: number, channelId: number): Promise<SignalAllocationActionResponse> {
    beginAllocationMutation()
    try {
      const workspaceId = requireWorkspaceId()
      const { data } = await SignalSheetAPI.swapAllocations(workspaceId, {
        signal_id: signalId,
        channel_id: channelId,
      })
      return applyAllocationActionResponse(data)
    } finally {
      endAllocationMutation()
    }
  }

  async function autoAllocate(payload: SignalAutoAllocatePayload) {
    devPerfIncrement("signalSheet.autoAllocate.calls")
    const endMeasure = devPerfMeasureStart("signalSheet.autoAllocate")
    const optimisticAssignments = resolveAutoAllocateAssignments(payload)
    const optimisticSignalIds = optimisticAssignments.map(item => item.signalId)
    const optimisticSignalVersions = new Map<number, number>()
    const rollback = new Map<number, SignalAllocationRow>()

    beginAllocationMutation()
    try {
      for (let index = 0; index < optimisticSignalIds.length; index += 1) {
        const signalId = optimisticSignalIds[index]
        allocationMutationVersionCounter += 1
        const nextVersion = allocationMutationVersionCounter
        allocationMutationVersionBySignalId.set(signalId, nextVersion)
        optimisticSignalVersions.set(signalId, nextVersion)
        const rowIndex = allocationIndexBySignalId.get(signalId)
        if (rowIndex !== undefined) {
          const row = allocationRows.value[rowIndex]
          if (row) {
            rollback.set(signalId, cloneAllocationRow(row))
          }
        }
        if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
          await yieldToEventLoop()
        }
      }

      if (optimisticAssignments.length > 0) {
        for (let index = 0; index < optimisticAssignments.length; index += 1) {
          const entry = optimisticAssignments[index]
          applyLocalAllocationPatch(entry.signalId, entry.channelId, { skipRevision: true })
          if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
            await yieldToEventLoop()
          }
        }
        setRecentlyChangedSignalIds(optimisticSignalIds)
        bumpAllocationRevision()
      }

      const workspaceId = requireWorkspaceId()
      const endApiMeasure = devPerfMeasureStart("signalSheet.autoAllocate.api")
      const { data } = await SignalSheetAPI.autoAllocate(workspaceId, {
        ...payload,
        prefer_single_unit: payload.prefer_single_unit ?? true,
      })
      endApiMeasure({
        workspaceId,
        optimisticAssignments: optimisticAssignments.length,
      })

      if (Array.isArray(data.rows) && data.rows.length > 0) {
        const serverSignalIds = data.rows.map(row => row.signal_id)
        const stillCurrentServerSignalIds = serverSignalIds.filter((signalId) => {
          const expectedVersion = optimisticSignalVersions.get(signalId)
          if (!expectedVersion) return true
          return allocationMutationVersionBySignalId.get(signalId) === expectedVersion
        })
        if (stillCurrentServerSignalIds.length > 0) {
          applyServerAllocationPatch(data.rows, stillCurrentServerSignalIds)
        }
      }

      if (optimisticSignalIds.length > 0) {
        const serverChangedSet = new Set<number>(Array.isArray(data.rows) ? data.rows.map(row => row.signal_id) : [])
        let rolledBack = false
        for (let index = 0; index < optimisticSignalIds.length; index += 1) {
          const signalId = optimisticSignalIds[index]
          if (serverChangedSet.has(signalId)) {
            if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
              await yieldToEventLoop()
            }
            continue
          }
          const expectedVersion = optimisticSignalVersions.get(signalId)
          if (!expectedVersion || allocationMutationVersionBySignalId.get(signalId) !== expectedVersion) {
            if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
              await yieldToEventLoop()
            }
            continue
          }
          const rollbackRow = rollback.get(signalId)
          if (!rollbackRow) {
            if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
              await yieldToEventLoop()
            }
            continue
          }
          const rowIndex = allocationIndexBySignalId.get(signalId)
          if (rowIndex === undefined) {
            if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
              await yieldToEventLoop()
            }
            continue
          }
          const row = allocationRows.value[rowIndex]
          if (!row) {
            if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
              await yieldToEventLoop()
            }
            continue
          }
          const previousAllocated = isAllocatedChannelId(row.channel_id)
          const previousChannelId = normalizeChannelId(row.channel_id)
          allocationRows.value[rowIndex] = cloneAllocationRow(rollbackRow)
          if (previousChannelId !== null && allocationOwnerByChannelId.get(previousChannelId) === signalId) {
            allocationOwnerByChannelId.delete(previousChannelId)
          }
          const nextChannelId = normalizeChannelId(rollbackRow.channel_id)
          if (nextChannelId !== null) {
            allocationOwnerByChannelId.set(nextChannelId, signalId)
          }
          const nextAllocated = isAllocatedChannelId(rollbackRow.channel_id)
          patchSheetAllocatedCount(previousAllocated, nextAllocated)
          rolledBack = true
          if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
            await yieldToEventLoop()
          }
        }
        if (rolledBack) {
          bumpAllocationRevision()
        }
      }

      recomputeSheetAllocatedCount()
      devPerfIncrement("signalSheet.autoAllocate.completed")
      endMeasure({
        ok: true,
        optimisticAssignments: optimisticAssignments.length,
        serverRows: Array.isArray(data.rows) ? data.rows.length : 0,
      })
      return data
    } catch (error) {
      if (optimisticSignalIds.length > 0) {
        const rollbackEntries = Array.from(rollback.entries())
        for (let index = 0; index < rollbackEntries.length; index += 1) {
          const [signalId, rollbackRow] = rollbackEntries[index]
          const expectedVersion = optimisticSignalVersions.get(signalId)
          if (!expectedVersion || allocationMutationVersionBySignalId.get(signalId) !== expectedVersion) {
            if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
              await yieldToEventLoop()
            }
            continue
          }
          const rowIndex = allocationIndexBySignalId.get(signalId)
          if (rowIndex === undefined) {
            if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
              await yieldToEventLoop()
            }
            continue
          }
          const row = allocationRows.value[rowIndex]
          if (!row) {
            if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
              await yieldToEventLoop()
            }
            continue
          }
          const previousAllocated = isAllocatedChannelId(row.channel_id)
          const previousChannelId = normalizeChannelId(row.channel_id)
          allocationRows.value[rowIndex] = cloneAllocationRow(rollbackRow)
          if (previousChannelId !== null && allocationOwnerByChannelId.get(previousChannelId) === signalId) {
            allocationOwnerByChannelId.delete(previousChannelId)
          }
          const nextChannelId = normalizeChannelId(rollbackRow.channel_id)
          if (nextChannelId !== null) {
            allocationOwnerByChannelId.set(nextChannelId, signalId)
          }
          const nextAllocated = isAllocatedChannelId(rollbackRow.channel_id)
          patchSheetAllocatedCount(previousAllocated, nextAllocated)
          if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
            await yieldToEventLoop()
          }
        }
        bumpAllocationRevision()
      }
      endMeasure({
        ok: false,
        optimisticAssignments: optimisticAssignments.length,
      })
      throw error
    } finally {
      endAllocationMutation()
    }
  }

  async function ensureAllocated(
    signalIds: number[],
    options?: { preferOnline?: boolean },
  ): Promise<SignalAllocationEnsureResponse> {
    devPerfIncrement("signalSheet.ensureAllocated.calls")
    const endMeasure = devPerfMeasureStart("signalSheet.ensureAllocated")
    const normalizedSignalIds = Array.from(
      new Set(signalIds.map(item => Number(item)).filter(id => Number.isFinite(id) && id > 0)),
    )
    const workspaceId = requireWorkspaceId()
    const endApiMeasure = devPerfMeasureStart("signalSheet.ensureAllocated.api")
    const { data } = await SignalSheetAPI.ensureAllocated(workspaceId, {
      signal_ids: normalizedSignalIds,
      prefer_online: options?.preferOnline ?? true,
    })
    endApiMeasure({ workspaceId, requested: normalizedSignalIds.length })
    if (Array.isArray(data.rows) && data.rows.length > 0) {
      applyServerAllocationPatch(
        data.rows,
        data.rows.map(row => row.signal_id),
      )
      recomputeSheetAllocatedCount()
    }
    devPerfIncrement("signalSheet.ensureAllocated.completed")
    endMeasure({
      requested: normalizedSignalIds.length,
      serverRows: Array.isArray(data.rows) ? data.rows.length : 0,
    })
    return data
  }

  async function markSignalsTested(
    signalIds: number[],
    options?: { optimistic?: boolean },
  ): Promise<void> {
    devPerfIncrement("signalSheet.markSignalsTested.calls")
    const endMeasure = devPerfMeasureStart("signalSheet.markSignalsTested")
    const normalizedSignalIds = Array.from(
      new Set(signalIds.map(item => Number(item)).filter(id => Number.isFinite(id) && id > 0)),
    )
    if (!normalizedSignalIds.length) {
      endMeasure({ skipped: true, reason: "empty-signal-ids" })
      return
    }

    const optimistic = options?.optimistic ?? true
    if (optimistic) {
      const testedAtIso = new Date().toISOString()
      applyLocalTestedAtPatch(normalizedSignalIds, testedAtIso)
    }

    const workspaceId = requireWorkspaceId()
    const endApiMeasure = devPerfMeasureStart("signalSheet.markSignalsTested.api")
    const { data } = await SignalSheetAPI.markTested(workspaceId, {
      signal_ids: normalizedSignalIds,
    })
    endApiMeasure({ workspaceId, requested: normalizedSignalIds.length })
    if (Array.isArray(data) && data.length > 0) {
      applyServerAllocationPatch(
        data,
        data.map(row => row.signal_id),
        {
          skipRecentlyChanged: true,
          skipRevision: true,
        },
      )
    }
    devPerfIncrement("signalSheet.markSignalsTested.completed")
    endMeasure({
      optimistic,
      requested: normalizedSignalIds.length,
      serverRows: Array.isArray(data) ? data.length : 0,
    })
  }

  function applyAllocationRowsPatch(rows: SignalAllocationRow[]) {
    if (!Array.isArray(rows) || rows.length === 0) {
      return
    }
    applyServerAllocationPatch(
      rows,
      rows.map(row => row.signal_id),
    )
  }

  const hasSheet = computed(() => Boolean(sheet.value && sheet.value.signals_count > 0))
  const allocatedCount = computed(() => {
    if (sheet.value) {
      return Math.max(0, Number(sheet.value.allocated_count ?? 0))
    }
    return allocationRows.value.filter(row => isAllocatedChannelId(row.channel_id)).length
  })
  const updatingAllocations = computed(() => allocationMutationsInFlight.value > 0)

  function getAllocationOwnerSignalId(channelId: number): number | null {
    return allocationOwnerByChannelId.get(channelId) ?? null
  }

  return {
    sheet,
    presets,
    allocationRows,
    loadingSheet,
    loadingPresets,
    loadingAllocations,
    updatingAllocations,
    importing,
    lastSheetLoadedAt,
    lastPresetsLoadedAt,
    lastAllocationsLoadedAt,
    allocationRevision,
    recentlyChangedSignalIds,
    hasSheet,
    allocatedCount,
    resetState,
    bootstrap,
    refreshSheet,
    ensureSheetLoaded,
    refreshPresets,
    ensurePresetsLoaded,
    refreshAllocations,
    ensureAllocationsLoaded,
    previewImportSheet,
    importSheet,
    savePreset,
    deletePreset,
    bulkSetAllocations,
    setAllocation,
    assignAllocation,
    reassignAllocation,
    unassignAllocation,
    swapAllocations,
    autoAllocate,
    ensureAllocated,
    markSignalsTested,
    applyTestedAtBySignalPatch,
    applyAllocationRowsPatch,
    getAllocationOwnerSignalId,
  }
})
