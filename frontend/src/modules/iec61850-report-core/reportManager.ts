import type {
  Iec61850DeviceEndpoint,
  Iec61850ReportConnection,
  Iec61850ReportControlCandidate,
  Iec61850ReportControlReadResult,
  Iec61850ReportControlRef,
  Iec61850ReportControlState,
  Iec61850ReportEvent,
  Iec61850ReportManagerAdapter,
  Iec61850ReportRuntimeDiagnostic,
} from "./types"

export class Iec61850ReportManager {
  constructor(private readonly adapter: Iec61850ReportManagerAdapter) {}

  async readReportControl(
    endpoint: Iec61850DeviceEndpoint,
    candidate: Iec61850ReportControlCandidate,
  ): Promise<Iec61850ReportControlReadResult> {
    return withConnection(this.adapter, endpoint, async (connection) => {
      const state = await connection.readReportControl(toReportControlRef(candidate))
      return {
        endpoint,
        candidateId: candidate.id,
        state,
        diagnostics: compareReportControlState(candidate, state),
      }
    })
  }

  async reserveReportControl(
    endpoint: Iec61850DeviceEndpoint,
    candidate: Iec61850ReportControlCandidate,
    clientId: string,
  ): Promise<Iec61850ReportControlState> {
    return withConnection(this.adapter, endpoint, connection =>
      connection.reserveReportControl(toReportControlRef(candidate), clientId),
    )
  }

  async releaseReportControl(
    endpoint: Iec61850DeviceEndpoint,
    candidate: Iec61850ReportControlCandidate,
    clientId: string,
  ): Promise<Iec61850ReportControlState> {
    return withConnection(this.adapter, endpoint, connection =>
      connection.releaseReportControl(toReportControlRef(candidate), clientId),
    )
  }

  async enableReportControl(
    endpoint: Iec61850DeviceEndpoint,
    candidate: Iec61850ReportControlCandidate,
    clientId: string,
  ): Promise<Iec61850ReportControlState> {
    return withConnection(this.adapter, endpoint, connection =>
      connection.enableReportControl(toReportControlRef(candidate), clientId),
    )
  }

  async disableReportControl(
    endpoint: Iec61850DeviceEndpoint,
    candidate: Iec61850ReportControlCandidate,
    clientId: string,
  ): Promise<Iec61850ReportControlState> {
    return withConnection(this.adapter, endpoint, connection =>
      connection.disableReportControl(toReportControlRef(candidate), clientId),
    )
  }

  async sendGeneralInterrogation(
    endpoint: Iec61850DeviceEndpoint,
    candidate: Iec61850ReportControlCandidate,
    clientId: string,
  ): Promise<Iec61850ReportEvent> {
    return withConnection(this.adapter, endpoint, connection =>
      connection.sendGeneralInterrogation(toReportControlRef(candidate), clientId),
    )
  }
}

export function toReportControlRef(candidate: Iec61850ReportControlCandidate): Iec61850ReportControlRef {
  return {
    iedName: candidate.iedName,
    accessPointName: candidate.accessPointName,
    logicalDeviceInst: candidate.logicalDeviceInst,
    logicalNodeName: candidate.logicalNodeName,
    reportControlName: candidate.reportControlName,
    reportKind: candidate.reportKind,
  }
}

export function compareReportControlState(
  candidate: Iec61850ReportControlCandidate,
  state: Iec61850ReportControlState,
): Iec61850ReportRuntimeDiagnostic[] {
  const reference = toReportControlRef(candidate)
  const diagnostics: Iec61850ReportRuntimeDiagnostic[] = []

  if (candidate.signalCount === 0) {
    diagnostics.push({
      severity: "warning",
      code: "DATASET_EMPTY",
      message: "ReportControl has no resolved DataSet signals in the SCD model.",
      reference,
    })
  }
  if (state.dataSetRef !== candidate.dataSetRef) {
    diagnostics.push({
      severity: "error",
      code: "DATASET_MISMATCH",
      message: `Live DataSet reference "${state.dataSetRef ?? ""}" does not match SCD "${candidate.dataSetRef ?? ""}".`,
      reference,
    })
  }
  if (state.confRev !== candidate.confRev) {
    diagnostics.push({
      severity: "error",
      code: "CONFREV_MISMATCH",
      message: `Live ConfRev "${state.confRev ?? ""}" does not match SCD "${candidate.confRev ?? ""}".`,
      reference,
    })
  }
  if (state.signalCount !== candidate.signalCount) {
    diagnostics.push({
      severity: "error",
      code: "SIGNAL_COUNT_MISMATCH",
      message: `Live DataSet signal count ${state.signalCount} does not match SCD ${candidate.signalCount}.`,
      reference,
    })
  }
  compareBooleanMap("TRGOPS_MISMATCH", "TrgOps", candidate.triggerOptions, state.triggerOptions, reference, diagnostics)
  compareBooleanMap("OPTFIELDS_MISMATCH", "OptFields", candidate.optionalFields, state.optionalFields, reference, diagnostics)

  return diagnostics
}

async function withConnection<T>(
  adapter: Iec61850ReportManagerAdapter,
  endpoint: Iec61850DeviceEndpoint,
  operation: (connection: Iec61850ReportConnection) => Promise<T>,
): Promise<T> {
  const connection = await adapter.connect(endpoint)
  try {
    return await operation(connection)
  } finally {
    await connection.disconnect()
  }
}

function compareBooleanMap(
  code: string,
  label: string,
  expected: Record<string, boolean | null>,
  actual: Record<string, boolean | null>,
  reference: Iec61850ReportControlRef,
  diagnostics: Iec61850ReportRuntimeDiagnostic[],
) {
  const keys = new Set([...Object.keys(expected), ...Object.keys(actual)])
  for (const key of keys) {
    if ((expected[key] ?? null) !== (actual[key] ?? null)) {
      diagnostics.push({
        severity: "warning",
        code,
        message: `Live ${label}.${key} "${actual[key] ?? ""}" does not match SCD "${expected[key] ?? ""}".`,
        reference,
      })
    }
  }
}
