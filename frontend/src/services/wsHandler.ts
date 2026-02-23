import { WSChannel } from '@/types/ws/events'
import { useDeviceStore } from '@/stores/deviceStore'
import { useChannelStore } from '@/stores/channelStore'
import { getLogger } from '@/utils/logger'
import { useSequenceStore } from '@/stores/sequenceStore'
import { useSystemHealthStore } from '@/stores/systemHealthStore'
import { useSignalJobStore } from '@/stores/signalJobStore'
import { useSignalSheetStore } from '@/stores/signalSheetStore'
import { useTestedAtRealtimeStore } from '@/stores/testedAtRealtimeStore'

const logger = getLogger('ws')
const TEST_RUN_JOB_EVENT_THROTTLE_MS = 150
const lastTestRunJobEventMetaByJobId = new Map<string, { at: number; status: string; updatedAt: string }>()
const pendingTestRunJobEventsById = new Map<string, SignalAllocationJobEvent | SignalTestRunJobEvent>()
let testRunJobFlushFrame: number | null = null
const pendingTestedAtPatchByJobId = new Map<string, Record<string, string>>()

import type {
  WSEvent,
  DeviceRegisterEvent,
  DeviceHeartbeatEvent,
  DeviceStateEvent,
  DeviceRespEvent,
  SequenceWsEvent,
  ChannelWSEvent,
  SystemHealthChangedEvent,
  SignalAllocationJobEvent,
  SignalTestRunJobEvent,
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
  if (!testedAtPatch || typeof testedAtPatch !== "object") {
    return
  }

  const testedAtPatchRecord = testedAtPatch as Record<string, string>
  const jobId = String(jobEvent.job_id)
  const isTerminal = isTerminalJobStatus(jobEvent.status)

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

export function handleWsEvent(event: WSEvent) {
  const deviceStore = useDeviceStore()
  const channelStore = useChannelStore()
  const sequenceStore = useSequenceStore()
  const systemHealthStore = useSystemHealthStore()
  const signalJobStore = useSignalJobStore()
  const signalSheetStore = useSignalSheetStore()
  const testedAtRealtimeStore = useTestedAtRealtimeStore()

  // Route sequence events into the sequence store so realtime progress stays in sync.
  if ('topic' in event && (event as SequenceWsEvent).topic === 'sequence') {
    sequenceStore.handleSequenceEvent(event as SequenceWsEvent)
    return
  }

  const channelEvent = event as ChannelWSEvent

  switch (channelEvent.channel) {

    case WSChannel.SYSTEM_INFO: {
      const sysEvent = channelEvent as SystemHealthChangedEvent
      if (sysEvent.event === "system_health_changed") {
        logger.debug("📡 IN ← SYSTEM_HEALTH:", sysEvent)
        systemHealthStore.applySnapshot(sysEvent.snapshot)
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
