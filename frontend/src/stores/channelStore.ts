// src/stores/channelStore.ts
import { defineStore } from "pinia"
import { ref } from "vue"

import { getLogger } from "@/utils/logger"

import { useDeviceStore } from "@/stores/deviceStore"

import { useChannelLogStore } from "@/stores/channelLogStore"
import type { ChannelLogEntry } from "@/stores/channelLogStore"

import {
  type DeviceRespEvent,
} from "@/types/ws/events"

import { CHANNEL_TYPES, type Channel, type DoChannel, type DoChannelUiState } from "@/types/channel"
import {
  describeDiDiagnosticUpdates,
  type DiDiagnosticsChange,
  type DoDiagnosticsBitmasks,
} from "@/stores/utils/diagnostics"
import { createChannelCommandRuntime } from "@/stores/channelStore/commandRuntime"
import { createChannelTransportActions } from "@/stores/channelStore/transportActions"
import { createChannelRuntimeReducer } from "@/stores/channelStore/runtimeReducer"
import { createChannelStateRequests } from "@/stores/channelStore/stateRequests"
import { createChannelCatalog } from "@/stores/channelStore/catalog"

const logger = getLogger("CHANNEL")

export const useChannelStore = defineStore("channelStore", () => {
  const channels = ref<Channel[]>([])
  const responses = ref<Record<string, DeviceRespEvent>>({})
  const stateRevision = ref(0)
  const lastStateChangedKeys = ref<Set<string>>(new Set())

  const isLoading = ref(false)
  const isLoaded = ref(false)
  const logStore = useChannelLogStore()

  const deviceLastStateAt = new Map<number, number>()

  function channelKey(deviceId: number, chIndex: number) {
    return `${deviceId}:${chIndex}`
  }

  function findDeviceById(deviceId: number) {
    return useDeviceStore().devices.find(d => d.id === deviceId)
  }

  function findDeviceByUnitId(unitId: string) {
    return useDeviceStore().devices.find(d => d.unit_id === unitId)
  }

  const DIGITAL_ON = "ON" as const
  const DIGITAL_OFF = "OFF" as const
  const DIGITAL_PENDING = "PENDING" as const

  type DigitalStableLabel = typeof DIGITAL_ON | typeof DIGITAL_OFF

  function toDigitalLabel(value: unknown): DigitalStableLabel {
    return value ? DIGITAL_ON : DIGITAL_OFF
  }

  function formatPendingSuffix(active = true) {
    return active ? ` (${DIGITAL_PENDING})` : ""
  }

  function summarizeDoChannels(deviceId: number) {
    const doChannels = channelsByDevice(deviceId).filter(
      ch => ch.type === CHANNEL_TYPES.DO,
    ) as DoChannel[]
    let on = 0
    let off = 0
    doChannels.forEach(ch => {
      if (ch.state) {
        on += 1
      } else {
        off += 1
      }
    })
    return { on, off, total: doChannels.length }
  }

  function summarizeMaskTargets(doChannels: DoChannel[], mask: number) {
    let on = 0
    let off = 0
    doChannels.forEach(ch => {
      const target = ((mask >> ch.index) & 1) === 1
      if (target) {
        on += 1
      } else {
        off += 1
      }
    })
    return { on, off, total: doChannels.length }
  }

  function formatSummary(summary: { on: number; off: number; total: number }) {
    if (summary.total === 0) {
      return "no DO channels"
    }
    return `${DIGITAL_ON}: ${summary.on}, ${DIGITAL_OFF}: ${summary.off}`
  }

  function decodePairLabels(state2b: number): [DigitalStableLabel, DigitalStableLabel] | null {
    switch (state2b & 0b11) {
      case 0b00:
        // Unknown: both outputs OFF
        return [DIGITAL_OFF, DIGITAL_OFF]
      case 0b01:
        // Open: A=ON, B=OFF
        return [DIGITAL_ON, DIGITAL_OFF]
      case 0b10:
        // Closed: A=OFF, B=ON
        return [DIGITAL_OFF, DIGITAL_ON]
      case 0b11:
        // Undefined: both outputs ON
        return [DIGITAL_ON, DIGITAL_ON]
    }
    return null
  }

  function decodePairTargets(state2b: number): [boolean, boolean] | null {
    const labels = decodePairLabels(state2b)
    if (!labels) return null
    return [labels[0] === DIGITAL_ON, labels[1] === DIGITAL_ON]
  }

  type ChannelLogPayload = Pick<ChannelLogEntry, "type" | "message" | "reason">

  function pushDeviceLog(deviceId: number, entry: ChannelLogPayload, actionId?: string) {
    logStore.push(deviceId, {
      ...entry,
      actionId,
    })
  }

  function summarizeDoDiagnostics(deviceId: number, diag: DoDiagnosticsBitmasks): string {
    const doChannels = channelsByDevice(deviceId).filter(
      ch => ch.type === CHANNEL_TYPES.DO,
    ) as DoChannel[]
    if (!doChannels.length) {
      return "no DO channels"
    }

    const counts = { open: 0, fault: 0, soft: 0 }
    doChannels.forEach(ch => {
      const bit = 1 << ch.index
      if (diag.open_mask & bit) counts.open += 1
      if (diag.fault_mask & bit) counts.fault += 1
      if (diag.soft_mask & bit) counts.soft += 1
    })

    return `open:${counts.open}, fault:${counts.fault}, soft:${counts.soft}`
  }

  function logDiDiagnosticChanges(deviceId: number, changes: DiDiagnosticsChange[]) {
    changes.forEach(change => {
      const { labels, alert } = describeDiDiagnosticUpdates(change.updates)
      if (!labels.length) {
        return
      }
      const channelLabel = resolveChannelLabel(change.channel)
      const prefix = `DI ${channelLabel}`
      pushDeviceLog(deviceId, {
        type: alert ? "error" : "state",
        message: `${prefix}: ${labels.join(", ")}`
      })
    })
  }

  function findDoChannel(deviceId: number, chIndex: number): DoChannel | undefined {
    const raw = channelByDeviceAndIndex(deviceId, chIndex)
    if (!raw || raw.type !== CHANNEL_TYPES.DO) {
      return undefined
    }
    return raw as DoChannel | undefined
  }

  let clearDoUiTimersBridge: ((ui: DoChannelUiState) => void) | null = null
  const catalog = createChannelCatalog({
    channels,
    isLoading,
    isLoaded,
    logger,
    channelKey,
    findDeviceById,
    clearDoUiTimers: (ui) => {
      clearDoUiTimersBridge?.(ui)
    },
  })
  const {
    rebuildChannelIndexes,
    channelsByDeviceFast,
    channelByDeviceAndIndex,
    fetchAll,
    ensureLoaded,
    ensureDeviceChannelsLoaded,
    invalidateDeviceChannels,
    channelsByDevice,
    hasDeviceChannels,
    updateChannelField,
    setBaseChannels,
    resetCatalog,
  } = catalog

  const { requestStates } = createChannelStateRequests({
    logger,
    findDeviceById,
  })

  const commandRuntime = createChannelCommandRuntime({
    channelsByDeviceFast,
    requestStates,
  })
  const {
    enqueueAction,
    removeAction,
    registerAoAction,
    peekAoAction,
    clearAoAction,
    clearDoUiTimers,
    enterDoPendingState,
    fulfillDoPendingState,
    hasPendingDoForDevice,
    scheduleDoStateRefreshIfPending,
    clearDoStateRefreshTimers,
  } = commandRuntime
  clearDoUiTimersBridge = clearDoUiTimers

  const transportActions = createChannelTransportActions({
    logger,
    channelsByDevice,
    findDoChannel,
    enqueueAction,
    enterDoPendingState,
    scheduleDoStateRefreshIfPending,
    registerAoAction,
    pushDeviceLog,
    toDigitalLabel,
    formatPendingSuffix,
    summarizeMaskTargets,
    formatSummary,
    decodePairTargets,
    decodePairLabels,
  })
  const {
    sendDoCommand,
    sendDoAllCommand,
    sendDoPairCommand,
    sendAoCommand,
  } = transportActions

  const runtimeReducer = createChannelRuntimeReducer({
    logger,
    channels,
    responses,
    stateRevision,
    lastStateChangedKeys,
    deviceLastStateAt,
    channelKey,
    channelByDeviceAndIndex,
    channelsByDeviceFast,
    channelsByDevice,
    findDeviceByUnitId,
    fulfillDoPendingState,
    peekAoAction,
    clearAoAction,
    removeAction,
    pushDeviceLog,
    toDigitalLabel,
    summarizeDoChannels,
    formatSummary,
    summarizeDoDiagnostics,
    logDiDiagnosticChanges,
    resolveChannelLabel,
  })
  const {
    setChannels,
    setResponse,
    didChannelChangeInLastRevision,
  } = runtimeReducer

  /* ----------------------------- FETCH ALL ----------------------------- */

  function reset() {
    clearDoStateRefreshTimers()
    deviceLastStateAt.clear()
    resetCatalog()
    responses.value = {}
    stateRevision.value = 0
    lastStateChangedKeys.value = new Set()
  }

  /* ----------------------------- COMMANDS ----------------------------- */

  /* --------------------------- RESOLVERS --------------------------- */

  function resolveUnitId(deviceId: number): string {
    const dev = findDeviceById(deviceId)
    return dev?.unit_id ?? `dev#${deviceId}`
  }

  function resolveUnitName(deviceId: number): string {
    const dev = findDeviceById(deviceId)
    return dev?.name?.trim() || dev?.unit_id || `dev#${deviceId}`
  }

  function resolveChannelLabel(ch: Channel): string {
    if (ch.resolved_name?.trim()) {
      return ch.resolved_name
    }
    if (ch.name?.trim()) {
      return ch.name
    }
    return `CH${ch.index + 1}`
  }

  function resolveChannelFullLabel(ch: Channel): string {
    return `${resolveUnitName(ch.device_id)}/${resolveChannelLabel(ch)}`
  }

  function hasPendingCommandForUnit(unitId: string): boolean {
    const device = findDeviceByUnitId(unitId)
    if (!device) {
      return false
    }
    return hasPendingDoForDevice(device.id)
  }

  return {
    channels,
    responses,
    stateRevision,
    isLoading,
    isLoaded,

    fetchAll,
    ensureLoaded,
    ensureDeviceChannelsLoaded,
    invalidateDeviceChannels,
    reset,

    channelsByDevice,
    hasDeviceChannels,
    setBaseChannels,
    setChannels,
    setResponse,
    requestStates,

    updateChannelField,

    sendDoCommand,
    sendDoAllCommand,
    sendDoPairCommand,
    sendAoCommand,

    resolveUnitId,
    resolveUnitName,
    resolveChannelLabel,
    resolveChannelFullLabel,
    didChannelChangeInLastRevision,
    hasPendingCommandForUnit,
  }
})
