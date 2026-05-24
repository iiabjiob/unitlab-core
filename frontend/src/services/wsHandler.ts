import { WSChannel } from '@/types/ws/events'
import { useDeviceStore } from '@/stores/deviceStore'
import { useChannelStore } from '@/stores/channelStore'
import { getLogger } from '@/utils/logger'
import { useSequenceStore } from '@/stores/sequenceStore'
import { useSystemHealthStore } from '@/stores/systemHealthStore'
import { useSignalJobStore } from '@/stores/signalJobStore'
import { useSignalRowsPatchStore } from '@/stores/signalRowsPatchStore'
import { useSignalSheetStore } from '@/stores/signalSheetStore'
import { useTestedAtRealtimeStore } from '@/stores/testedAtRealtimeStore'
import { useCoreNetworkStore } from '@/stores/coreNetworkStore'
import { useCoreNtpStore } from '@/stores/coreNtpStore'
import { useCoreDiagnosticsStore } from '@/stores/coreDiagnosticsStore'
import { useCoreProvisionStore } from '@/stores/coreProvisionStore'
import { useToastStore } from '@/stores/toastStore'
import type { SystemHealthResponse } from '@/types/health'
import type { CoreDiagnosticsSnapshot } from '@/types/coreDiagnostics'
import router from '@/router'

const logger = getLogger('ws')
const TEST_RUN_JOB_EVENT_THROTTLE_MS = 150
const lastTestRunJobEventMetaByJobId = new Map<string, { at: number; status: string; updatedAt: string }>()
const pendingTestRunJobEventsById = new Map<string, SignalAllocationJobEvent | SignalTestRunJobEvent>()
let testRunJobFlushFrame: number | null = null
const pendingTestedAtPatchByJobId = new Map<string, Record<string, string>>()
const systemHealthCriticalToastState = {
  id: null as number | null,
  signature: null as string | null,
}
const coreDiagnosticsCriticalToastState = {
  id: null as number | null,
  signature: null as string | null,
}

import type {
  WSEvent,
  DeviceRegisterEvent,
  DeviceHeartbeatEvent,
  DeviceStateEvent,
  DeviceRespEvent,
  SequenceWsEvent,
  ChannelWSEvent,
  SystemHealthChangedEvent,
  CoreNetworkStateWsEvent,
  CoreNtpStateWsEvent,
  CoreDiagnosticsStateWsEvent,
  CoreProvisionStateWsEvent,
  SignalAllocationJobEvent,
  SignalTestRunJobEvent,
  SignalTestRuntimePatchEvent,
  SignalRowsPatchedEvent,
} from '@/types/ws/events'

function isTestRunJobEvent(jobEvent: SignalAllocationJobEvent | SignalTestRunJobEvent): boolean {
  return jobEvent.event === "signal_test_run_job" || String(jobEvent.operation) === "test_run"
}

function isTerminalJobStatus(status: unknown): boolean {
  const normalized = String(status ?? "")
  return normalized === "succeeded" || normalized === "failed" || normalized === "cancelled"
}

function shouldProcessTestRunJobEvent(jobEvent: SignalAllocationJobEvent | SignalTestRunJobEvent): boolean {
  if (!isTestRunJobEvent(jobEvent)) {
    return true
  }

  const status = String(jobEvent.status ?? "")
  const isTerminal = isTerminalJobStatus(status)
  if (isTerminal) {
    lastTestRunJobEventMetaByJobId.delete(String(jobEvent.job_id))
    return true
  }

  const now = Date.now()
  const jobId = String(jobEvent.job_id)
  const updatedAt = String(jobEvent.updated_at ?? "")
  const prev = lastTestRunJobEventMetaByJobId.get(jobId)

  if (prev) {
    const unchangedState = prev.status === status && prev.updatedAt === updatedAt
    if (unchangedState || now - prev.at < TEST_RUN_JOB_EVENT_THROTTLE_MS) {
      return false
    }
  }

  lastTestRunJobEventMetaByJobId.set(jobId, { at: now, status, updatedAt })
  return true
}

type JobEventStores = {
  signalJobStore: ReturnType<typeof useSignalJobStore>
  signalSheetStore: ReturnType<typeof useSignalSheetStore>
  testedAtRealtimeStore: ReturnType<typeof useTestedAtRealtimeStore>
}

function applySignalJobEvent(
  jobEvent: SignalAllocationJobEvent | SignalTestRunJobEvent,
  stores: JobEventStores,
) {
  const { signalJobStore, signalSheetStore, testedAtRealtimeStore } = stores

  signalJobStore.applyJobEvent(jobEvent)

  if (!isTestRunJobEvent(jobEvent)) {
    return
  }

  const result = (jobEvent.result ?? {}) as Record<string, unknown>
  const testedAtPatch = ["tested_at_patch", "tested_at_by_signal"]
    .map(key => result[key])
    .find(value => value && typeof value === "object" && !Array.isArray(value))
  const jobId = String(jobEvent.job_id)
  const isTerminal = isTerminalJobStatus(jobEvent.status)

  if (!testedAtPatch || typeof testedAtPatch !== "object") {
    if (isTerminal) {
      const pendingForJob = pendingTestedAtPatchByJobId.get(jobId)
      pendingTestedAtPatchByJobId.delete(jobId)
      if (pendingForJob && Object.keys(pendingForJob).length > 0) {
        signalSheetStore.applyTestedAtBySignalPatch(pendingForJob)
      }
    }
    return
  }

  const testedAtPatchRecord = testedAtPatch as Record<string, string>

  testedAtRealtimeStore.applyPatch(jobEvent.workspace_id, testedAtPatchRecord, { flush: "microtask" })

  if (isTerminal) {
    const pendingForJob = pendingTestedAtPatchByJobId.get(jobId)
    pendingTestedAtPatchByJobId.delete(jobId)
    const mergedPatch: Record<string, string> = {
      ...(pendingForJob ?? {}),
      ...testedAtPatchRecord,
    }
    signalSheetStore.applyTestedAtBySignalPatch(mergedPatch)
    return
  }

  const previousPatch = pendingTestedAtPatchByJobId.get(jobId)
  pendingTestedAtPatchByJobId.set(jobId, {
    ...(previousPatch ?? {}),
    ...testedAtPatchRecord,
  })
}

function applySignalTestRuntimePatch(
  event: SignalTestRuntimePatchEvent,
  stores: JobEventStores,
) {
  if (event.patch_type !== "tested_at") {
    return
  }

  const testedAtPatch = event.tested_at_by_signal
  if (!testedAtPatch || typeof testedAtPatch !== "object" || Array.isArray(testedAtPatch)) {
    return
  }

  const normalizedPatch: Record<string, string> = {}
  Object.entries(testedAtPatch).forEach(([signalId, testedAt]) => {
    const normalizedSignalId = String(signalId ?? "").trim()
    const normalizedTestedAt = String(testedAt ?? "").trim()
    if (!normalizedSignalId || !normalizedTestedAt) {
      return
    }
    normalizedPatch[normalizedSignalId] = normalizedTestedAt
  })

  if (Object.keys(normalizedPatch).length === 0) {
    return
  }

  stores.testedAtRealtimeStore.applyPatch(event.workspace_id, normalizedPatch, { flush: "microtask" })

  const jobId = String(event.job_id ?? "").trim()
  if (!jobId) {
    return
  }
  const previousPatch = pendingTestedAtPatchByJobId.get(jobId)
  pendingTestedAtPatchByJobId.set(jobId, {
    ...(previousPatch ?? {}),
    ...normalizedPatch,
  })
}

function flushPendingTestRunJobEvents(stores: JobEventStores) {
  if (pendingTestRunJobEventsById.size === 0) {
    return
  }

  const events = Array.from(pendingTestRunJobEventsById.values())
  pendingTestRunJobEventsById.clear()

  events.forEach((jobEvent) => {
    applySignalJobEvent(jobEvent, stores)
  })
}

function scheduleTestRunJobFlush(stores: JobEventStores) {
  if (testRunJobFlushFrame !== null) {
    return
  }

  testRunJobFlushFrame = requestAnimationFrame(() => {
    testRunJobFlushFrame = null
    flushPendingTestRunJobEvents(stores)
  })
}

function syncStickyCriticalToast(
  toastStore: ReturnType<typeof useToastStore>,
  state: { id: number | null; signature: string | null },
  nextSignature: string | null,
  message: string,
) {
  if (!nextSignature) {
    if (state.id !== null) {
      toastStore.remove(state.id)
      state.id = null
    }
    state.signature = null
    return
  }

  if (state.signature === nextSignature && state.id !== null) {
    return
  }

  if (state.id !== null) {
    toastStore.remove(state.id)
    state.id = null
  }

  state.id = toastStore.error(message, { timeout: null })
  state.signature = nextSignature
}

function syncSystemHealthCriticalAlert(
  toastStore: ReturnType<typeof useToastStore>,
  snapshot: SystemHealthResponse,
) {
  const status = String(snapshot.status ?? "online").toLowerCase()
  const issues = Array.isArray(snapshot.issues) ? snapshot.issues.map(item => String(item).trim()).filter(Boolean) : []
  const isCritical = status !== "online" || issues.length > 0

  if (!isCritical) {
    syncStickyCriticalToast(toastStore, systemHealthCriticalToastState, null, "")
    return
  }

  const signature = JSON.stringify({ status, issues })
  const issuePreview = issues.slice(0, 3).join("; ")
  const message = issuePreview
    ? `System ${status.toUpperCase()}: ${issuePreview}`
    : `System ${status.toUpperCase()}: critical health condition detected`

  if (systemHealthCriticalToastState.signature === signature && systemHealthCriticalToastState.id !== null) {
    return
  }
  if (systemHealthCriticalToastState.id !== null) {
    toastStore.remove(systemHealthCriticalToastState.id)
    systemHealthCriticalToastState.id = null
  }
  systemHealthCriticalToastState.id = toastStore.error(message, {
    timeout: null,
    actionLabel: "Open Diagnostics",
    onAction: () => {
      void router.push({ name: "settings.diagnostics" }).catch(() => undefined)
    },
  })
  systemHealthCriticalToastState.signature = signature
}

function syncCoreDiagnosticsCriticalAlert(
  toastStore: ReturnType<typeof useToastStore>,
  snapshot: CoreDiagnosticsSnapshot,
) {
  const issues: string[] = []
  const mode = String(snapshot.mode ?? "unknown").toLowerCase()
  if (mode === "error" || mode === "degraded") {
    issues.push(`mode ${mode.toUpperCase()}`)
  }

  const cpuTemp = Number(snapshot.cpu?.temperature_c)
  if (Number.isFinite(cpuTemp) && cpuTemp >= 85) {
    issues.push(`CPU temp ${cpuTemp.toFixed(1)}°C`)
  }

  const memoryUsed = Number(snapshot.memory?.used_percent)
  if (Number.isFinite(memoryUsed) && memoryUsed >= 95) {
    issues.push(`memory ${memoryUsed.toFixed(1)}%`)
  }

  const diskUsed = Number(snapshot.disk_root?.used_percent)
  if (Number.isFinite(diskUsed) && diskUsed >= 95) {
    issues.push(`disk ${diskUsed.toFixed(1)}%`)
  }

  const inactiveServices = (Array.isArray(snapshot.services) ? snapshot.services : [])
    .filter(service => service.active === false)
    .map(service => String(service.name ?? "").trim())
    .filter(Boolean)
  if (inactiveServices.length > 0) {
    issues.push(`services inactive (${inactiveServices.slice(0, 3).join(", ")}${inactiveServices.length > 3 ? ", …" : ""})`)
  }

  if (issues.length === 0) {
    syncStickyCriticalToast(toastStore, coreDiagnosticsCriticalToastState, null, "")
    return
  }

  const signature = JSON.stringify({ mode, issues })
  if (coreDiagnosticsCriticalToastState.signature === signature && coreDiagnosticsCriticalToastState.id !== null) {
    return
  }
  if (coreDiagnosticsCriticalToastState.id !== null) {
    toastStore.remove(coreDiagnosticsCriticalToastState.id)
    coreDiagnosticsCriticalToastState.id = null
  }
  coreDiagnosticsCriticalToastState.id = toastStore.error(`Core diagnostics alert: ${issues.join("; ")}`, {
    timeout: null,
    actionLabel: "Open Diagnostics",
    onAction: () => {
      void router.push({ name: "settings.diagnostics" }).catch(() => undefined)
    },
  })
  coreDiagnosticsCriticalToastState.signature = signature
}

export function handleWsEvent(event: WSEvent) {
  const deviceStore = useDeviceStore()
  const channelStore = useChannelStore()
  const sequenceStore = useSequenceStore()
  const systemHealthStore = useSystemHealthStore()
  const signalJobStore = useSignalJobStore()
  const signalRowsPatchStore = useSignalRowsPatchStore()
  const signalSheetStore = useSignalSheetStore()
  const testedAtRealtimeStore = useTestedAtRealtimeStore()
  const coreNetworkStore = useCoreNetworkStore()
  const coreNtpStore = useCoreNtpStore()
  const coreDiagnosticsStore = useCoreDiagnosticsStore()
  const coreProvisionStore = useCoreProvisionStore()
  const toastStore = useToastStore()

  // Route sequence events into the sequence store so realtime progress stays in sync.
  if ('topic' in event && (event as SequenceWsEvent).topic === 'sequence') {
    sequenceStore.handleSequenceEvent(event as SequenceWsEvent)
    return
  }

  const channelEvent = event as ChannelWSEvent

  switch (channelEvent.channel) {

    case WSChannel.SYSTEM_INFO: {
      const sysEvent = channelEvent as
        | SystemHealthChangedEvent
        | CoreNetworkStateWsEvent
        | CoreNtpStateWsEvent
        | CoreDiagnosticsStateWsEvent
        | CoreProvisionStateWsEvent
        | SignalAllocationJobEvent
        | SignalTestRunJobEvent
        | SignalTestRuntimePatchEvent
        | SignalRowsPatchedEvent
      if (sysEvent.event === "system_health_changed") {
        logger.debug("📡 IN ← SYSTEM_HEALTH:", sysEvent)
        systemHealthStore.applySnapshot(sysEvent.snapshot)
        syncSystemHealthCriticalAlert(toastStore, sysEvent.snapshot)
        break
      }
      if (sysEvent.event === "core_network_state") {
        logger.debug("📡 IN ← CORE_NETWORK_STATE:", sysEvent)
        coreNetworkStore.applySnapshot((sysEvent as CoreNetworkStateWsEvent).snapshot)
        break
      }
      if (sysEvent.event === "core_ntp_state") {
        logger.debug("📡 IN ← CORE_NTP_STATE:", sysEvent)
        coreNtpStore.applySnapshot((sysEvent as CoreNtpStateWsEvent).snapshot)
        break
      }
      if (sysEvent.event === "core_diagnostics_state") {
        logger.debug("📡 IN ← CORE_DIAGNOSTICS_STATE:", sysEvent)
        const diagnosticsSnapshot = (sysEvent as CoreDiagnosticsStateWsEvent).snapshot
        coreDiagnosticsStore.applySnapshot(diagnosticsSnapshot)
        syncCoreDiagnosticsCriticalAlert(toastStore, diagnosticsSnapshot)
        break
      }
      if (sysEvent.event === "core_provision_state") {
        logger.debug("📡 IN ← CORE_PROVISION_STATE:", sysEvent)
        coreProvisionStore.applySnapshot((sysEvent as CoreProvisionStateWsEvent).snapshot)
        break
      }
      if (sysEvent.event === "signal_test_runtime_patch") {
        applySignalTestRuntimePatch(sysEvent as SignalTestRuntimePatchEvent, {
          signalJobStore,
          signalSheetStore,
          testedAtRealtimeStore,
        })
        break
      }
      if (sysEvent.event === "signal_rows_patched") {
        signalRowsPatchStore.applyEvent(sysEvent as SignalRowsPatchedEvent)
        break
      }
      if (
        (channelEvent as SignalAllocationJobEvent).event === "signal_allocation_job"
        || (channelEvent as SignalTestRunJobEvent).event === "signal_test_run_job"
      ) {
        const jobEvent = channelEvent as SignalAllocationJobEvent | SignalTestRunJobEvent
        if (!isTestRunJobEvent(jobEvent)) {
          logger.debug("📡 IN ← SIGNAL_ALLOCATION_JOB:", jobEvent)
        }

        if (!shouldProcessTestRunJobEvent(jobEvent)) {
          break
        }

        const stores: JobEventStores = {
          signalJobStore,
          signalSheetStore,
          testedAtRealtimeStore,
        }

        if (isTestRunJobEvent(jobEvent) && !isTerminalJobStatus(jobEvent.status)) {
          pendingTestRunJobEventsById.set(String(jobEvent.job_id), jobEvent)
          scheduleTestRunJobFlush(stores)
          break
        }

        pendingTestRunJobEventsById.delete(String(jobEvent.job_id))
        flushPendingTestRunJobEvents(stores)
        applySignalJobEvent(jobEvent, stores)
        break
      }
      logger.warn("⚠️ Unknown SYSTEM_INFO payload", sysEvent)
      break
    }

    // Device registration: hydrate or merge the newly discovered unit.
    case WSChannel.DEVICE_REGISTER:{
      const devEvent = channelEvent as DeviceRegisterEvent
      logger.debug("📡 IN ← DEVICE_REGISTER:", devEvent)
      const parsedId = Number(devEvent.id)
      const fallbackId = Number(deviceStore.devices.find(item => item.unit_id === devEvent.unit_id)?.id)
      const deviceId = Number.isFinite(parsedId)
        ? parsedId
        : (Number.isFinite(fallbackId) ? fallbackId : devEvent.id)

      // Update or insert the device record first.
      deviceStore.upsertDevice({
        id: deviceId,
        unit_id: devEvent.unit_id,
        device_type: devEvent.device_type,
        num_channels: devEvent.num_channels ?? null,
        firmware_version: devEvent.firmware_version ?? null,
        name: devEvent.name ?? null,
        status: devEvent.status,
        last_seen: devEvent.last_seen ?? null,
        registered_at: devEvent.registered_at ?? null,
        channels: null,
      })

      // Hydrate channel catalog once when firmware includes channel descriptors.
      if (
        devEvent.channels &&
        Number.isFinite(Number(deviceId)) &&
        !channelStore.hasDeviceChannels(Number(deviceId))
      ) {
        channelStore.setBaseChannels(Number(deviceId), devEvent.channels)
      }
      break
    }
    // Heartbeat status updates keep online/offline indicators responsive.
    case WSChannel.DEVICE_STATUS:{
      logger.debug("📡 IN ← DEVICE_STATUS:", channelEvent)
      deviceStore.updateStatus(channelEvent as DeviceHeartbeatEvent)
      break
    }
    // Device state broadcasts carry DI/DO/AO changes for visualization.
    case WSChannel.DEVICE_STATE:{
      logger.debug("📡 IN ← DEVICE_STATE:", channelEvent)
      channelStore.setChannels(channelEvent as DeviceStateEvent)
      break
    }
    // Command responses confirm that the device acknowledged the instruction.
    case WSChannel.DEVICE_RESP: {
      logger.debug("📡 IN ← DEVICE_RESP:", channelEvent)
      channelStore.setResponse(channelEvent as DeviceRespEvent)
      break
    }
    default:
      logger.warn("⚠️ Unknown WS channel:", channelEvent)
  }
}
