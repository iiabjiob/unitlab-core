import { defineStore } from "pinia"
import { computed, ref } from "vue"

import { SignalSheetAPI } from "@/api/signal_sheet.api"
import type {
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
  const importing = ref(false)
  const allocationRevision = ref(0)

  const initializedWorkspaceId = ref<number | null>(null)
  let allocationsInFlight: Promise<SignalAllocationRow[]> | null = null
  const allocationIndexBySignalId = new Map<number, number>()

  function requireWorkspaceId(): number {
    return workspaceStore.requireWorkspaceId()
  }

  function bumpAllocationRevision() {
    allocationRevision.value += 1
  }

  function isFiniteChannelId(value: unknown): value is number {
    return typeof value === "number" && Number.isFinite(value)
  }

  function isAllocatedChannelId(value: unknown): boolean {
    return isFiniteChannelId(value)
  }

  function cloneAllocationRow(row: SignalAllocationRow): SignalAllocationRow {
    return {
      ...row,
      signal_metadata: row.signal_metadata && typeof row.signal_metadata === "object"
        ? { ...row.signal_metadata }
        : {},
    }
  }

  function rebuildAllocationIndex() {
    allocationIndexBySignalId.clear()
    allocationRows.value.forEach((row, index) => {
      allocationIndexBySignalId.set(row.signal_id, index)
    })
  }

  function replaceAllocationRows(rows: SignalAllocationRow[]) {
    allocationRows.value = rows
    rebuildAllocationIndex()
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

  function assignAllocationRow(targetRow: SignalAllocationRow, sourceRow: SignalAllocationRow) {
    const previousAllocated = isAllocatedChannelId(targetRow.channel_id)
    Object.assign(targetRow, sourceRow)
    const nextAllocated = isAllocatedChannelId(targetRow.channel_id)
    patchSheetAllocatedCount(previousAllocated, nextAllocated)
  }

  function resolveChannelLabel(unitId: string | null, channelIndex: number | null): string | null {
    if (!Number.isFinite(channelIndex as number)) {
      return null
    }
    const suffix = `ch${Number(channelIndex) + 1}`
    return unitId ? `${unitId}/${suffix}` : suffix
  }

  function applyLocalAllocationPatch(signalId: number, nextChannelIdRaw: number | null | undefined) {
    const rowIndex = allocationIndexBySignalId.get(signalId)
    if (rowIndex === undefined) {
      return
    }
    const row = allocationRows.value[rowIndex]
    if (!row) {
      return
    }

    const previousAllocated = isAllocatedChannelId(row.channel_id)
    const nextChannelId = isFiniteChannelId(nextChannelIdRaw) ? nextChannelIdRaw : null

    if (nextChannelId === null) {
      row.channel_id = null
      row.channel_type = null
      row.channel_index = null
      row.channel_label = null
      row.device_id = null
      row.unit_id = null
      row.unit_online = null
      row.unit_last_seen_at = null
    } else {
      const channel = channelStore.channels.find(item => item.id === nextChannelId)
      row.channel_id = nextChannelId
      if (!channel) {
        row.channel_type = null
        row.channel_index = null
        row.channel_label = null
        row.device_id = null
        row.unit_id = null
        row.unit_online = null
        row.unit_last_seen_at = null
      } else {
        const device = deviceStore.devices.find(item => item.id === channel.device_id)
        const unitId = device?.unit_id ?? channelStore.resolveUnitId(channel.device_id) ?? null
        row.channel_type = channel.type
        row.channel_index = channel.index
        row.device_id = channel.device_id
        row.unit_id = unitId
        row.channel_label = resolveChannelLabel(unitId, channel.index)
        row.unit_online = device ? device.status === "online" : null
        row.unit_last_seen_at = null
      }
    }

    const nextAllocated = isAllocatedChannelId(row.channel_id)
    patchSheetAllocatedCount(previousAllocated, nextAllocated)
    bumpAllocationRevision()
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
      assignAllocationRow(targetRow, serverRow)
    })
    bumpAllocationRevision()
  }

  function resetState() {
    sheet.value = null
    presets.value = []
    allocationRows.value = []
    allocationIndexBySignalId.clear()
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

    const affectedSignalIds = effectiveEntries.map(item => item.signal_id)
    const rollback = new Map<number, SignalAllocationRow>()
    affectedSignalIds.forEach((signalId) => {
      const rowIndex = allocationIndexBySignalId.get(signalId)
      if (rowIndex === undefined) return
      const row = allocationRows.value[rowIndex]
      if (!row) return
      rollback.set(signalId, cloneAllocationRow(row))
    })

    effectiveEntries.forEach((entry) => {
      applyLocalAllocationPatch(entry.signal_id, entry.channel_id)
    })

    const workspaceId = requireWorkspaceId()
    try {
      const { data } = await SignalSheetAPI.updateAllocations(workspaceId, effectiveEntries)
      applyServerAllocationPatch(data, affectedSignalIds)
      return allocationRows.value
    } catch (error) {
      rollback.forEach((snapshot, signalId) => {
        const rowIndex = allocationIndexBySignalId.get(signalId)
        if (rowIndex === undefined) return
        const row = allocationRows.value[rowIndex]
        if (!row) return
        assignAllocationRow(row, snapshot)
      })
      bumpAllocationRevision()
      throw error
    }
  }

  async function setAllocation(signalId: number, channelId: number | null) {
    return bulkSetAllocations([{ signal_id: signalId, channel_id: channelId }])
  }

  async function autoAllocate(payload: SignalAutoAllocatePayload) {
    const workspaceId = requireWorkspaceId()
    const { data } = await SignalSheetAPI.autoAllocate(workspaceId, payload)
    replaceAllocationRows(data.rows)
    recomputeSheetAllocatedCount()
    return data
  }

  const hasSheet = computed(() => Boolean(sheet.value && sheet.value.signals_count > 0))
  const allocatedCount = computed(() => allocationRows.value.filter(row => isAllocatedChannelId(row.channel_id)).length)

  return {
    sheet,
    presets,
    allocationRows,
    loadingSheet,
    loadingPresets,
    loadingAllocations,
    importing,
    allocationRevision,
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
  }
})
