export type Iec61850StandardDocumentId =
  | "IEC_61850_6_2024"
  | "IEC_61850_7_2_2020"
  | "IEC_61850_8_1_2020"

export type Iec61850StandardTermStatus =
  | "implemented"
  | "planned"
  | "adapter-owned"
  | "internal-only"

export type Iec61850StandardTerm = {
  standardName: string
  standardDocument: Iec61850StandardDocumentId | null
  internalName: string | null
  status: Iec61850StandardTermStatus
  notes: string
}

export const IEC61850_REPORT_STANDARD_DOCUMENTS: readonly {
  id: Iec61850StandardDocumentId
  path: string
  role: string
}[] = [
  {
    id: "IEC_61850_6_2024",
    path: "/workspace/docs/.IEC61850/IEC 61850-6-2024.pdf",
    role: "SCL/SCD source model: IED, AccessPoint, Server, LDevice, LN0/LN, DataSet, ReportControl, TrgOps, OptFields, RptEnabled.",
  },
  {
    id: "IEC_61850_7_2_2020",
    path: "/workspace/docs/.IEC61850/IEC 61850-7-2-2020.pdf",
    role: "ACSI report semantics: DataSet, BRCB/URCB, trigger options, optional fields, reservation, enable, GI, report payload fields.",
  },
  {
    id: "IEC_61850_8_1_2020",
    path: "/workspace/docs/.IEC61850/IEC 61850-8-1-2020.pdf",
    role: "MMS mapping for future real adapter object references and report service payloads.",
  },
] as const

export const IEC61850_SCL_REPORT_TERMS: readonly Iec61850StandardTerm[] = [
  term("ReportControl", "IEC_61850_6_2024", "SclReportControl", "implemented", "Parsed from SCL LN0/LN scope."),
  term("DataSet", "IEC_61850_6_2024", "SclDataSet", "implemented", "Parsed and linked to ReportControl when resolvable in logical-device scope."),
  term("FCDA", "IEC_61850_6_2024", "SclDataSetMember.kind", "implemented", "Parsed as a DataSet member."),
  term("FCD", "IEC_61850_6_2024", "SclDataSetMember.kind", "implemented", "Parsed as a DataSet member."),
  term("TrgOps", "IEC_61850_6_2024", "SclReportTriggerOptions", "implemented", "SCL trigger options parsed from ReportControl child element."),
  term("OptFields", "IEC_61850_6_2024", "SclReportOptionalFields", "implemented", "SCL optional fields parsed from ReportControl child element."),
  term("RptEnabled", "IEC_61850_6_2024", "SclReportEnabled", "implemented", "Parsed as SCD inventory only; runtime reservation is adapter/session owned."),
  term("ClientLN", "IEC_61850_6_2024", "SclReportClient", "implemented", "Parsed as SCD inventory only."),
] as const

export const IEC61850_REPORT_CONTROL_ATTRIBUTE_TERMS: readonly Iec61850StandardTerm[] = [
  term("RptID", "IEC_61850_7_2_2020", "Iec61850ReportControlState.rptId", "implemented", "Read/compare field for report control state."),
  term("RptEna", "IEC_61850_7_2_2020", "Iec61850ReportControlState.enabled", "implemented", "Simulator models enable/disable; real write belongs to backend adapter."),
  term("DatSet", "IEC_61850_7_2_2020", "Iec61850ReportControlState.dataSetRef", "implemented", "Compared against SCD DataSet reference."),
  term("ConfRev", "IEC_61850_7_2_2020", "Iec61850ReportControlState.confRev", "implemented", "Compared before activation and report normalization."),
  term("OptFlds", "IEC_61850_7_2_2020", "Iec61850ReportControlState.optionalFields", "implemented", "Read/compare map; individual report fields are normalized separately."),
  term("TrgOps", "IEC_61850_7_2_2020", "Iec61850ReportControlState.triggerOptions", "implemented", "Read/compare map."),
  term("BufTm", "IEC_61850_7_2_2020", "Iec61850ReportControlState.bufferTimeMs", "implemented", "Carried from SCD/live state; write behavior is future adapter-owned."),
  term("IntgPd", "IEC_61850_7_2_2020", "Iec61850ReportControlState.integrityPeriodMs", "implemented", "Carried from SCD/live state; write behavior is future adapter-owned."),
  term("GI", "IEC_61850_7_2_2020", "Iec61850ReportConnection.sendGeneralInterrogation", "implemented", "Simulator service request normalizes the resulting report event."),
  term("SqNum", "IEC_61850_7_2_2020", "Iec61850ReportControlState.sequenceNumber", "implemented", "Simulator state and normalized report event carry sequence number."),
  term("EntryID", "IEC_61850_7_2_2020", "Iec61850ReportEvent.entryId", "implemented", "Normalized report payload field."),
  term("TimeOfEntry", "IEC_61850_7_2_2020", "Iec61850ReportEvent.timeOfEntry", "implemented", "Normalized report payload field."),
  term("Owner", "IEC_61850_7_2_2020", "Iec61850ReportControlState.owner", "implemented", "Simulator reservation/enable ownership model; real ownership is adapter-owned."),
  term("Resv", "IEC_61850_7_2_2020", "Iec61850ReportControlState.reservedBy", "implemented", "Simulator reservation identity; real service semantics are adapter-owned."),
  term("ResvTms", "IEC_61850_7_2_2020", null, "planned", "Buffered report timed reservation is not implemented in simulator yet."),
  term("PurgeBuf", "IEC_61850_7_2_2020", null, "planned", "Buffered report purge behavior is not implemented in simulator yet."),
] as const

export const IEC61850_TRIGGER_OPTION_TERMS: readonly Iec61850StandardTerm[] = [
  term("dchg", "IEC_61850_7_2_2020", "dataChange", "implemented", "Mapped to internal report reason data-change."),
  term("qchg", "IEC_61850_7_2_2020", "qualityChange", "implemented", "Mapped to internal report reason quality-change."),
  term("dupd", "IEC_61850_7_2_2020", "dataUpdate", "implemented", "Mapped to internal report reason data-update."),
  term("period", "IEC_61850_7_2_2020", "periodic", "implemented", "Mapped to internal report reason integrity."),
  term("gi", "IEC_61850_7_2_2020", "generalInterrogation", "implemented", "Mapped to internal report reason general-interrogation."),
] as const

export const IEC61850_OPTIONAL_FIELD_TERMS: readonly Iec61850StandardTerm[] = [
  term("seqNum", "IEC_61850_6_2024", "sequenceNumber", "implemented", "SCL OptFields attribute; runtime report field is SqNum."),
  term("timeStamp", "IEC_61850_6_2024", "timestamp", "implemented", "SCL OptFields attribute; runtime report field is TimeOfEntry/value timestamp."),
  term("reasonCode", "IEC_61850_6_2024", "reasonCode", "implemented", "SCL OptFields attribute; normalized report value carries reasonCode."),
  term("dataSet", "IEC_61850_6_2024", "dataSetName", "implemented", "SCL OptFields attribute; normalized report event carries dataSetRef."),
  term("dataRef", "IEC_61850_6_2024", "dataReference", "implemented", "SCL OptFields attribute; normalized values carry dataReference."),
  term("entryID", "IEC_61850_6_2024", "entryId", "implemented", "SCL OptFields attribute; normalized report event carries entryId."),
  term("configRef", "IEC_61850_6_2024", "configRevision", "implemented", "SCL OptFields attribute; normalized report event carries confRev."),
  term("bufOvfl", "IEC_61850_6_2024", "bufferOverflow", "implemented", "SCL OptFields attribute; normalized report event carries bufferOverflow."),
] as const

export const IEC61850_REPORT_PAYLOAD_FIELD_TERMS: readonly Iec61850StandardTerm[] = [
  term("SqNum", "IEC_61850_7_2_2020", "Iec61850ReportEvent.sequenceNumber", "implemented", "Optional report field when present."),
  term("TimeOfEntry", "IEC_61850_7_2_2020", "Iec61850ReportEvent.timeOfEntry", "implemented", "Optional report field when present."),
  term("ReasonForInclusion", "IEC_61850_7_2_2020", "Iec61850ReportValue.reasonCode", "implemented", "Represented by portable report reason strings."),
  term("DataSet", "IEC_61850_7_2_2020", "Iec61850ReportEvent.dataSetRef", "implemented", "Optional report field when present."),
  term("DataRef", "IEC_61850_7_2_2020", "Iec61850ReportValue.dataReference", "implemented", "Optional value-level field when present."),
  term("EntryID", "IEC_61850_7_2_2020", "Iec61850ReportEvent.entryId", "implemented", "Optional report field when present."),
  term("ConfRev", "IEC_61850_7_2_2020", "Iec61850ReportEvent.confRev", "implemented", "Optional report field when present."),
  term("BufOvfl", "IEC_61850_7_2_2020", "Iec61850ReportEvent.bufferOverflow", "implemented", "Optional report field when present."),
] as const

export const UNITLAB_INTERNAL_REPORT_TERMS: readonly Iec61850StandardTerm[] = [
  term("lifecycleState", null, "Iec61850ReportControlState.lifecycleState", "internal-only", "UnitLab simulator/session diagnostic state; not an IEC attribute."),
  term("simulatorEventLog", null, "Iec61850SimulatorEvent", "internal-only", "UnitLab validation evidence; not an IEC service payload."),
  term("diagnostics", null, "Iec61850ReportRuntimeDiagnostic", "internal-only", "UnitLab validation output; not an IEC attribute."),
  term("signalObservations", null, "Iec61850SignalObservation", "internal-only", "UnitLab FAT signal-level evidence derived from normalized report values; not an IEC attribute."),
] as const

export function getIec61850ReportComplianceTerms(): readonly Iec61850StandardTerm[] {
  return [
    ...IEC61850_SCL_REPORT_TERMS,
    ...IEC61850_REPORT_CONTROL_ATTRIBUTE_TERMS,
    ...IEC61850_TRIGGER_OPTION_TERMS,
    ...IEC61850_OPTIONAL_FIELD_TERMS,
    ...IEC61850_REPORT_PAYLOAD_FIELD_TERMS,
    ...UNITLAB_INTERNAL_REPORT_TERMS,
  ]
}

function term(
  standardName: string,
  standardDocument: Iec61850StandardDocumentId | null,
  internalName: string | null,
  status: Iec61850StandardTermStatus,
  notes: string,
): Iec61850StandardTerm {
  return {
    standardName,
    standardDocument,
    internalName,
    status,
    notes,
  }
}
