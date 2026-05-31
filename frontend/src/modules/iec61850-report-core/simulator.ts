import type {
  Iec61850DeviceEndpoint,
  Iec61850ReportLifecycleState,
  Iec61850ReportConnection,
  Iec61850ReportControlCandidate,
  Iec61850ReportControlRef,
  Iec61850ReportControlState,
  Iec61850ReportManagerAdapter,
} from "./types"
import { normalizeIec61850ReportEvent } from "./reportEventNormalizer"
import { toReportControlRef } from "./reportManager"

export type Iec61850SimulatorDevice = {
  endpoint: Iec61850DeviceEndpoint
  reports: Iec61850ReportControlCandidate[]
  overrides?: Record<string, Partial<Iec61850ReportControlState>>
}

export type Iec61850SimulatorAdapterOptions = {
  devices: Iec61850SimulatorDevice[]
  now?: () => Date
  strictDisconnectWhileEnabled?: boolean
}

export type Iec61850SimulatorEventKind =
  | "connect"
  | "disconnect"
  | "read"
  | "reserve"
  | "release"
  | "enable"
  | "disable"
  | "general-interrogation"
  | "report"
  | "failure"

export type Iec61850SimulatorEvent = {
  id: string
  at: string
  endpointId: string
  reportControlKey: string | null
  kind: Iec61850SimulatorEventKind
  lifecycleState: Iec61850ReportLifecycleState
  clientId: string | null
  code: string | null
  message: string | null
}

export type Iec61850SimulatorAdapter = Iec61850ReportManagerAdapter & {
  getEventLog(): Iec61850SimulatorEvent[]
  clearEventLog(): void
}

export class Iec61850SimulatorStateError extends Error {
  constructor(
    readonly code: string,
    message: string,
  ) {
    super(message)
    this.name = "Iec61850SimulatorStateError"
  }
}

type SimulatorDeviceState = {
  endpoint: Iec61850DeviceEndpoint
  reports: Map<string, SimulatorReportState>
}

type SimulatorReportState = {
  candidate: Iec61850ReportControlCandidate
  expectedConfRev: string | null
  signalRefs: string[]
  state: Iec61850ReportControlState
}

type SimulatorEventSink = {
  append(
    device: SimulatorDeviceState,
    reportKey: string | null,
    kind: Iec61850SimulatorEventKind,
    lifecycleState: Iec61850ReportLifecycleState,
    clientId?: string | null,
    code?: string | null,
    message?: string | null,
  ): void
}

export function createIec61850SimulatorAdapter(
  options: Iec61850SimulatorAdapterOptions,
): Iec61850SimulatorAdapter {
  const now = options.now ?? (() => new Date())
  const eventLog: Iec61850SimulatorEvent[] = []
  let eventSequence = 0
  const events: SimulatorEventSink = {
    append(device, reportKey, kind, lifecycleState, clientId = null, code = null, message = null) {
      const at = now().toISOString()
      eventSequence += 1
      eventLog.push({
        id: `${device.endpoint.id}:${eventSequence}`,
        at,
        endpointId: device.endpoint.id,
        reportControlKey: reportKey,
        kind,
        lifecycleState,
        clientId,
        code,
        message,
      })
    },
  }
  const devices = new Map<string, SimulatorDeviceState>()

  for (const device of options.devices) {
    const reports = new Map<string, SimulatorReportState>()
    for (const candidate of device.reports) {
      const reference = toReportControlRef(candidate)
      const key = reportControlKey(reference)
      const override = device.overrides?.[key] ?? {}
      reports.set(key, {
        candidate,
        expectedConfRev: candidate.confRev,
        signalRefs: candidate.normalizedSignals.map(signal => signal.reference),
        state: {
          ...stateFromCandidate(candidate),
          ...override,
          reference: override.reference ?? reference,
        },
      })
    }
    devices.set(device.endpoint.id, { endpoint: device.endpoint, reports })
  }

  return {
    async connect(endpoint) {
      if (endpoint.mode !== "simulator") {
        throw new Error("IEC 61850 simulator adapter only accepts simulator endpoints.")
      }
      const device = devices.get(endpoint.id)
      if (!device) {
        throw new Error(`IEC 61850 simulator endpoint "${endpoint.id}" is not registered.`)
      }
      events.append(device, null, "connect", "connected")
      return createConnection(device, now, events, options.strictDisconnectWhileEnabled === true)
    },
    getEventLog() {
      return eventLog.map(event => ({ ...event }))
    },
    clearEventLog() {
      eventLog.splice(0, eventLog.length)
    },
  }
}

function createConnection(
  device: SimulatorDeviceState,
  now: () => Date,
  events: SimulatorEventSink,
  strictDisconnectWhileEnabled: boolean,
): Iec61850ReportConnection {
  let connected = true

  return {
    async readReportControl(reference) {
      const runtime = readReport(device, reference)
      assertConnected(connected, runtime, device, events)
      transition(runtime, device, events, "read", "read")
      return cloneState(runtime.state)
    },
    async reserveReportControl(reference, clientId) {
      const runtime = readReport(device, reference)
      assertConnected(connected, runtime, device, events)
      const { state } = runtime
      if (state.enabled) {
        fail(runtime, device, events, "RESERVE_WHILE_ENABLED", "ReportControl must be disabled before reservation changes.", clientId)
      }
      if (state.reservedBy && state.reservedBy !== clientId) {
        fail(runtime, device, events, "RESERVATION_CONFLICT", `ReportControl is already reserved by "${state.reservedBy}".`, clientId)
      }
      state.reservedBy = clientId
      state.owner = clientId
      transition(runtime, device, events, "reserve", "reserved", clientId)
      return cloneState(state)
    },
    async releaseReportControl(reference, clientId) {
      const runtime = readReport(device, reference)
      assertConnected(connected, runtime, device, events)
      const { state } = runtime
      if (state.enabled) {
        fail(runtime, device, events, "RELEASE_WHILE_ENABLED", "ReportControl must be disabled before reservation release.", clientId)
      }
      if (state.reservedBy && state.reservedBy !== clientId) {
        fail(runtime, device, events, "RESERVATION_CONFLICT", `ReportControl is reserved by "${state.reservedBy}".`, clientId)
      }
      state.reservedBy = null
      state.owner = null
      transition(runtime, device, events, "release", "released", clientId)
      return cloneState(state)
    },
    async enableReportControl(reference, clientId) {
      const runtime = readReport(device, reference)
      assertConnected(connected, runtime, device, events)
      const { state } = runtime
      if (state.confRev !== runtime.expectedConfRev) {
        fail(runtime, device, events, "CONFREV_STALE", `Live ConfRev "${state.confRev ?? ""}" does not match SCD "${runtime.expectedConfRev ?? ""}".`, clientId)
      }
      if (!state.reservedBy) {
        fail(runtime, device, events, "ENABLE_WITHOUT_RESERVATION", "ReportControl must be reserved before enable.", clientId)
      }
      if (state.reservedBy && state.reservedBy !== clientId) {
        fail(runtime, device, events, "RESERVATION_CONFLICT", `ReportControl is reserved by "${state.reservedBy}".`, clientId)
      }
      state.enabled = true
      state.owner = clientId
      transition(runtime, device, events, "enable", "enabled", clientId)
      return cloneState(state)
    },
    async disableReportControl(reference, clientId) {
      const runtime = readReport(device, reference)
      assertConnected(connected, runtime, device, events)
      const { state } = runtime
      if (state.owner && state.owner !== clientId) {
        fail(runtime, device, events, "OWNERSHIP_CONFLICT", `ReportControl is owned by "${state.owner}".`, clientId)
      }
      state.enabled = false
      transition(runtime, device, events, "disable", "disabled", clientId)
      return cloneState(state)
    },
    async sendGeneralInterrogation(reference, clientId) {
      const runtime = readReport(device, reference)
      assertConnected(connected, runtime, device, events)
      const { state } = runtime
      if (!state.enabled) {
        fail(runtime, device, events, "GI_WHILE_DISABLED", "ReportControl must be enabled before GI.", clientId)
      }
      if (state.owner && state.owner !== clientId) {
        fail(runtime, device, events, "OWNERSHIP_CONFLICT", `ReportControl is owned by "${state.owner}".`, clientId)
      }
      state.giInProgress = true
      transition(runtime, device, events, "general-interrogation", "gi-pending", clientId)
      state.sequenceNumber += 1
      const receivedAt = now().toISOString()
      const result = normalizeIec61850ReportEvent({
        endpointId: device.endpoint.id,
        candidate: runtime.candidate,
        reportControl: reference,
        receivedAt,
        payload: {
          rptId: state.rptId,
          dataSetRef: state.dataSetRef,
          confRev: state.confRev,
          sequenceNumber: state.sequenceNumber,
          timeOfEntry: receivedAt,
          entryId: `${device.endpoint.id}:${reportControlKey(reference)}:${state.sequenceNumber}`,
          bufferOverflow: false,
          reason: "general-interrogation",
          values: runtime.signalRefs.map((signalRef, index) => ({
            dataReference: signalRef,
            value: index,
            reasonCode: "general-interrogation",
            timestamp: receivedAt,
          })),
        },
      })
      const event = result.event
      state.giInProgress = false
      transition(runtime, device, events, "report", "reporting", clientId)
      return event
    },
    async disconnect() {
      if (!connected) {
        return
      }
      connected = false
      events.append(device, null, "disconnect", "disconnected")
      if (!strictDisconnectWhileEnabled) {
        return
      }
      for (const runtime of device.reports.values()) {
        if (runtime.state.enabled) {
          fail(runtime, device, events, "DISCONNECT_WHILE_ENABLED", "Connection disconnected while a ReportControl was still enabled.")
        }
      }
    },
  }
}

function stateFromCandidate(candidate: Iec61850ReportControlCandidate): Iec61850ReportControlState {
  return {
    reference: toReportControlRef(candidate),
    lifecycleState: "disconnected",
    rptId: candidate.rptId,
    dataSetRef: candidate.dataSetRef,
    confRev: candidate.confRev,
    indexed: candidate.indexed,
    bufferTimeMs: candidate.bufferTimeMs,
    integrityPeriodMs: candidate.integrityPeriodMs,
    triggerOptions: { ...candidate.triggerOptions },
    optionalFields: { ...candidate.optionalFields },
    signalCount: candidate.signalCount,
    enabled: false,
    reservedBy: null,
    owner: null,
    sequenceNumber: 0,
    giInProgress: false,
  }
}

function readReport(device: SimulatorDeviceState, reference: Iec61850ReportControlRef): SimulatorReportState {
  const key = reportControlKey(reference)
  const runtime = device.reports.get(key)
  if (!runtime) {
    throw new Error(`ReportControl "${key}" is not available on simulator endpoint "${device.endpoint.id}".`)
  }
  return runtime
}

function assertConnected(
  connected: boolean,
  runtime: SimulatorReportState,
  device: SimulatorDeviceState,
  events: SimulatorEventSink,
): asserts connected is true {
  if (!connected) {
    fail(runtime, device, events, "CONNECTION_CLOSED", "Simulator connection is already disconnected.")
  }
}

function transition(
  runtime: SimulatorReportState,
  device: SimulatorDeviceState,
  events: SimulatorEventSink,
  kind: Iec61850SimulatorEventKind,
  lifecycleState: Iec61850ReportLifecycleState,
  clientId: string | null = null,
) {
  runtime.state.lifecycleState = lifecycleState
  events.append(device, reportControlKey(runtime.state.reference), kind, lifecycleState, clientId)
}

function fail(
  runtime: SimulatorReportState,
  device: SimulatorDeviceState,
  events: SimulatorEventSink,
  code: string,
  message: string,
  clientId: string | null = null,
): never {
  runtime.state.lifecycleState = "failed"
  events.append(device, reportControlKey(runtime.state.reference), "failure", "failed", clientId, code, message)
  throw new Iec61850SimulatorStateError(code, message)
}

function cloneState(state: Iec61850ReportControlState): Iec61850ReportControlState {
  return {
    ...state,
    reference: { ...state.reference },
    triggerOptions: { ...state.triggerOptions },
    optionalFields: { ...state.optionalFields },
  }
}

export function reportControlKey(reference: Iec61850ReportControlRef): string {
  return [
    reference.iedName,
    reference.accessPointName,
    reference.logicalDeviceInst,
    reference.logicalNodeName,
    reference.reportControlName,
    reference.reportKind,
  ].join("/")
}
