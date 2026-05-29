import type {
  Iec61850DeviceEndpoint,
  Iec61850ReportConnection,
  Iec61850ReportControlCandidate,
  Iec61850ReportControlRef,
  Iec61850ReportControlState,
  Iec61850ReportEvent,
  Iec61850ReportManagerAdapter,
  Iec61850ReportValue,
} from "./types"
import { toReportControlRef } from "./reportManager"

export type Iec61850SimulatorDevice = {
  endpoint: Iec61850DeviceEndpoint
  reports: Iec61850ReportControlCandidate[]
  overrides?: Record<string, Partial<Iec61850ReportControlState>>
}

export type Iec61850SimulatorAdapterOptions = {
  devices: Iec61850SimulatorDevice[]
  now?: () => Date
}

type SimulatorDeviceState = {
  endpoint: Iec61850DeviceEndpoint
  reports: Map<string, Iec61850ReportControlState>
  signals: Map<string, string[]>
}

export function createIec61850SimulatorAdapter(
  options: Iec61850SimulatorAdapterOptions,
): Iec61850ReportManagerAdapter {
  const now = options.now ?? (() => new Date())
  const devices = new Map<string, SimulatorDeviceState>()

  for (const device of options.devices) {
    const reports = new Map<string, Iec61850ReportControlState>()
    const signals = new Map<string, string[]>()
    for (const candidate of device.reports) {
      const reference = toReportControlRef(candidate)
      const key = reportControlKey(reference)
      const override = device.overrides?.[key] ?? {}
      reports.set(key, {
        ...stateFromCandidate(candidate),
        ...override,
        reference: override.reference ?? reference,
      })
      signals.set(key, candidate.signals.map(signal => signal.reference))
    }
    devices.set(device.endpoint.id, { endpoint: device.endpoint, reports, signals })
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
      return createConnection(device, now)
    },
  }
}

function createConnection(device: SimulatorDeviceState, now: () => Date): Iec61850ReportConnection {
  return {
    async readReportControl(reference) {
      return cloneState(readState(device, reference))
    },
    async reserveReportControl(reference, clientId) {
      const state = readState(device, reference)
      if (state.enabled) {
        throw new Error("ReportControl must be disabled before reservation changes.")
      }
      if (state.reservedBy && state.reservedBy !== clientId) {
        throw new Error(`ReportControl is already reserved by "${state.reservedBy}".`)
      }
      state.reservedBy = clientId
      state.owner = clientId
      return cloneState(state)
    },
    async releaseReportControl(reference, clientId) {
      const state = readState(device, reference)
      if (state.enabled) {
        throw new Error("ReportControl must be disabled before reservation release.")
      }
      if (state.reservedBy && state.reservedBy !== clientId) {
        throw new Error(`ReportControl is reserved by "${state.reservedBy}".`)
      }
      state.reservedBy = null
      state.owner = null
      return cloneState(state)
    },
    async enableReportControl(reference, clientId) {
      const state = readState(device, reference)
      if (state.reservedBy && state.reservedBy !== clientId) {
        throw new Error(`ReportControl is reserved by "${state.reservedBy}".`)
      }
      state.enabled = true
      state.owner = clientId
      return cloneState(state)
    },
    async disableReportControl(reference, clientId) {
      const state = readState(device, reference)
      if (state.owner && state.owner !== clientId) {
        throw new Error(`ReportControl is owned by "${state.owner}".`)
      }
      state.enabled = false
      return cloneState(state)
    },
    async sendGeneralInterrogation(reference, clientId) {
      const state = readState(device, reference)
      if (!state.enabled) {
        throw new Error("ReportControl must be enabled before GI.")
      }
      if (state.owner && state.owner !== clientId) {
        throw new Error(`ReportControl is owned by "${state.owner}".`)
      }
      state.giInProgress = true
      state.sequenceNumber += 1
      const receivedAt = now().toISOString()
      const values = (device.signals.get(reportControlKey(reference)) ?? []).map((signalRef, index): Iec61850ReportValue => ({
        reference: signalRef,
        value: index,
        reasonCode: "general-interrogation",
        timestamp: receivedAt,
      }))
      state.giInProgress = false
      return {
        id: `${device.endpoint.id}:${reportControlKey(reference)}:${state.sequenceNumber}`,
        endpointId: device.endpoint.id,
        receivedAt,
        reportControl: reference,
        rptId: state.rptId,
        dataSetRef: state.dataSetRef,
        confRev: state.confRev,
        sequenceNumber: state.sequenceNumber,
        reason: "general-interrogation",
        values,
      }
    },
    async disconnect() {},
  }
}

function stateFromCandidate(candidate: Iec61850ReportControlCandidate): Iec61850ReportControlState {
  return {
    reference: toReportControlRef(candidate),
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

function readState(device: SimulatorDeviceState, reference: Iec61850ReportControlRef): Iec61850ReportControlState {
  const key = reportControlKey(reference)
  const state = device.reports.get(key)
  if (!state) {
    throw new Error(`ReportControl "${key}" is not available on simulator endpoint "${device.endpoint.id}".`)
  }
  return state
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
