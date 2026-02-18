import { defineStore } from "pinia"
import { computed, ref } from "vue"

import { SignalSheetAPI } from "@/api/signal_sheet.api"
import type {
  SignalAllocationEnsureResponse,
  SignalAllocationRow,
  SignalAllocationUpdateItem,
  SignalAutoAllocatePayload,
  SignalImportMeta,
  SignalSheet,
  SignalSheetPreset,
} from "@/types/signal"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { getLogger } from "@/utils/logger"

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
  const lastAllocationsLoadedAt = ref<number | null>(null)
  const allocationRevision = ref(0)
  const recentlyChangedSignalIds = ref<number[]>([])

  const initializedWorkspaceId = ref<number | null>(null)
  let allocationsInFlight: Promise<SignalAllocationRow[]> | null = null
  const allocationIndexBySignalId = new Map<number, number>()
  const allocationOwnerByChannelId = new Map<number, number>()
  const allocationMutationVersionBySignalId = new Map<number, number>()
  let allocationMutationVersionCounter = 0
  const ALLOCATION_BATCH_SIZE = 200

  function requireWorkspaceId(): number {
    return workspaceStore.requireWorkspaceId()
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

  function isAllocatedChannelId(value: unknown): boolean {
    return isFiniteChannelId(value)
  }

  function normalizeChannelId(value: unknown): number | null {
    return isFiniteChannelId(value) ? value : null
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

  async function yieldToEventLoop() {
    await new Promise<void>((resolve) => {
      setTimeout(resolve, 0)
    })
  }

  function normalizeDirection(value: unknown): "DI" | "DO" | "AI" | "AO" | null {
    const normalized = String(value ?? "").trim().toUpperCase()
    if (normalized === "DI" || normalized === "DO" || normalized === "AI" || normalized === "AO") {
      return normalized
    }
    return null
  }

  function requiredChannelTypeForDirection(value: unknown): "di" | "do" | "ai" | "ao" | null {
    const direction = normalizeDirection(value)
    if (direction === "DI") return "di"
    if (direction === "DO") return "do"
    if (direction === "AI") return "ai"
    if (direction === "AO") return "ao"
    return null
  }

  function normalizedChannelType(value: unknown): "di" | "do" | "ai" | "ao" | null {
    const normalized = String(value ?? "").trim().toLowerCase()
    if (normalized.startsWith("di")) return "di"
    if (normalized.startsWith("do")) return "do"
    if (normalized.startsWith("ai")) return "ai"
    if (normalized.startsWith("ao")) return "ao"
    return null
  }

  function isDeviceOnline(deviceId: number): boolean {
    const device = deviceStore.devices.find(item => item.id === deviceId)
    return device?.status === "online"
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

    const sortedChannels = [...channelStore.channels].sort((left, right) => {
      const onlineLeft = preferOnline && isDeviceOnline(left.device_id) ? 0 : 1
      const onlineRight = preferOnline && isDeviceOnline(right.device_id) ? 0 : 1
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

      const requiredType = requiredChannelTypeForDirection(row.signal_direction)
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

  function replaceAllocationRows(rows: SignalAllocationRow[]) {
    allocationRows.value = rows
    rebuildAllocationIndexes()
    setRecentlyChangedSignalIds(rows.map(row => row.signal_id))
    bumpAllocationRevision()
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
    const touched: number[] = []
    signalIds.forEach((signalId) => {
      const rowIndex = allocationIndexBySignalId.get(signalId)
      if (rowIndex === undefined) return
      const row = allocationRows.value[rowIndex]
      if (!row) return
      allocationRows.value[rowIndex] = {
        ...row,
        tested_at: testedAtIso,
      }
      touched.push(signalId)
    })
    if (!touched.length) {
      return
    }
    setRecentlyChangedSignalIds(touched)
    bumpAllocationRevision()
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
    const nextChannelId = isFiniteChannelId(nextChannelIdRaw) ? nextChannelIdRaw : null
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

  function applyServerAllocationPatch(serverRows: SignalAllocationRow[], signalIds: readonly number[]) {
    if (!signalIds.length) {
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
      replaceAllocationRows(serverRows)
      recomputeSheetAllocatedCount()
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
      const nextRow = {
        ...targetRow,
        ...serverRow,
        signal_metadata: serverRow.signal_metadata && typeof serverRow.signal_metadata === "object"
          ? { ...serverRow.signal_metadata }
          : {},
      }
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
    setRecentlyChangedSignalIds(signalIds)
    bumpAllocationRevision()
  }

  function resetState() {
    sheet.value = null
    presets.value = []
    allocationRows.value = []
    lastSheetLoadedAt.value = null
    lastAllocationsLoadedAt.value = null
    allocationIndexBySignalId.clear()
    allocationOwnerByChannelId.clear()
    allocationMutationVersionBySignalId.clear()
    setRecentlyChangedSignalIds([])
    initializedWorkspaceId.value = null
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
    const workspaceId = requireWorkspaceId()
    loadingSheet.value = true
    try {
      const { data } = await SignalSheetAPI.get(workspaceId)
      sheet.value = data
      lastSheetLoadedAt.value = Date.now()
      return data
    } finally {
      loadingSheet.value = false
    }
  }

  async function refreshPresets() {
    const workspaceId = requireWorkspaceId()
    loadingPresets.value = true
    try {
      const { data } = await SignalSheetAPI.listPresets(workspaceId)
      presets.value = data
      return data
    } finally {
      loadingPresets.value = false
    }
  }

  async function refreshAllocations() {
    if (allocationsInFlight) {
      return allocationsInFlight
    }

    const workspaceId = requireWorkspaceId()
    const task = (async () => {
      loadingAllocations.value = true
      try {
        const { data } = await SignalSheetAPI.listAllocations(workspaceId)
        replaceAllocationRows(data)
        recomputeSheetAllocatedCount()
        lastAllocationsLoadedAt.value = Date.now()
        return data
      } finally {
        loadingAllocations.value = false
      }
    })()

    allocationsInFlight = task
    try {
      return await task
    } finally {
      if (allocationsInFlight === task) {
        allocationsInFlight = null
      }
    }
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
    if (!entries.length) {
      return allocationRows.value
    }

    const deduped = new Map<number, number | null>()
    entries.forEach((entry) => {
      const normalizedChannelId = isFiniteChannelId(entry.channel_id) ? entry.channel_id : null
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
      const current = isFiniteChannelId(row.channel_id) ? row.channel_id : null
      return current !== entry.channel_id
    })
    if (!effectiveEntries.length) {
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
      const { data } = await SignalSheetAPI.updateAllocations(workspaceId, effectiveEntries)
      const stillCurrentSignalIds = affectedSignalIds.filter((signalId) => (
        allocationMutationVersionBySignalId.get(signalId) === affectedSignalVersions.get(signalId)
      ))
      if (stillCurrentSignalIds.length > 0) {
        applyServerAllocationPatch(data, stillCurrentSignalIds)
      }
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
        const snapshotRow = rollback.get(signalId)
        if (rowIndex === undefined || !snapshotRow) {
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
        allocationRows.value[rowIndex] = cloneAllocationRow(snapshotRow)
        if (previousChannelId !== null && allocationOwnerByChannelId.get(previousChannelId) === signalId) {
          allocationOwnerByChannelId.delete(previousChannelId)
        }
        const nextChannelId = normalizeChannelId(snapshotRow.channel_id)
        if (nextChannelId !== null) {
          allocationOwnerByChannelId.set(nextChannelId, signalId)
        }
        const nextAllocated = isAllocatedChannelId(snapshotRow.channel_id)
        patchSheetAllocatedCount(previousAllocated, nextAllocated)
        if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
          await yieldToEventLoop()
        }
      }
      bumpAllocationRevision()
      throw error
    } finally {
      endAllocationMutation()
    }
  }

  async function setAllocation(signalId: number, channelId: number | null) {
    return bulkSetAllocations([{ signal_id: signalId, channel_id: channelId }])
  }

  async function autoAllocate(payload: SignalAutoAllocatePayload) {
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
      const { data } = await SignalSheetAPI.autoAllocate(workspaceId, {
        ...payload,
        prefer_single_unit: payload.prefer_single_unit ?? true,
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
          const snapshot = rollback.get(signalId)
          if (!snapshot) {
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
          allocationRows.value[rowIndex] = cloneAllocationRow(snapshot)
          if (previousChannelId !== null && allocationOwnerByChannelId.get(previousChannelId) === signalId) {
            allocationOwnerByChannelId.delete(previousChannelId)
          }
          const nextChannelId = normalizeChannelId(snapshot.channel_id)
          if (nextChannelId !== null) {
            allocationOwnerByChannelId.set(nextChannelId, signalId)
          }
          const nextAllocated = isAllocatedChannelId(snapshot.channel_id)
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
      return data
    } catch (error) {
      if (optimisticSignalIds.length > 0) {
        const rollbackEntries = Array.from(rollback.entries())
        for (let index = 0; index < rollbackEntries.length; index += 1) {
          const [signalId, snapshot] = rollbackEntries[index]
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
          allocationRows.value[rowIndex] = cloneAllocationRow(snapshot)
          if (previousChannelId !== null && allocationOwnerByChannelId.get(previousChannelId) === signalId) {
            allocationOwnerByChannelId.delete(previousChannelId)
          }
          const nextChannelId = normalizeChannelId(snapshot.channel_id)
          if (nextChannelId !== null) {
            allocationOwnerByChannelId.set(nextChannelId, signalId)
          }
          const nextAllocated = isAllocatedChannelId(snapshot.channel_id)
          patchSheetAllocatedCount(previousAllocated, nextAllocated)
          if ((index + 1) % ALLOCATION_BATCH_SIZE === 0) {
            await yieldToEventLoop()
          }
        }
        bumpAllocationRevision()
      }
      throw error
    } finally {
      endAllocationMutation()
    }
  }

  async function ensureAllocated(
    signalIds: number[],
    options?: { preferOnline?: boolean },
  ): Promise<SignalAllocationEnsureResponse> {
    const normalizedSignalIds = Array.from(
      new Set(signalIds.map(item => Number(item)).filter(id => Number.isFinite(id) && id > 0)),
    )
    const workspaceId = requireWorkspaceId()
    const { data } = await SignalSheetAPI.ensureAllocated(workspaceId, {
      signal_ids: normalizedSignalIds,
      prefer_online: options?.preferOnline ?? true,
    })
    if (Array.isArray(data.rows) && data.rows.length > 0) {
      applyServerAllocationPatch(
        data.rows,
        data.rows.map(row => row.signal_id),
      )
      recomputeSheetAllocatedCount()
    }
    return data
  }

  async function markSignalsTested(signalIds: number[]): Promise<void> {
    const normalizedSignalIds = Array.from(
      new Set(signalIds.map(item => Number(item)).filter(id => Number.isFinite(id) && id > 0)),
    )
    if (!normalizedSignalIds.length) {
      return
    }

    const testedAtIso = new Date().toISOString()
    applyLocalTestedAtPatch(normalizedSignalIds, testedAtIso)

    const workspaceId = requireWorkspaceId()
    const { data } = await SignalSheetAPI.markTested(workspaceId, {
      signal_ids: normalizedSignalIds,
    })
    if (Array.isArray(data) && data.length > 0) {
      applyServerAllocationPatch(
        data,
        data.map(row => row.signal_id),
      )
    }
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
    lastAllocationsLoadedAt,
    allocationRevision,
    recentlyChangedSignalIds,
    hasSheet,
    allocatedCount,
    resetState,
    bootstrap,
    refreshSheet,
    refreshPresets,
    refreshAllocations,
    importSheet,
    savePreset,
    deletePreset,
    bulkSetAllocations,
    setAllocation,
    autoAllocate,
    ensureAllocated,
    markSignalsTested,
    applyAllocationRowsPatch,
    getAllocationOwnerSignalId,
  }
})
