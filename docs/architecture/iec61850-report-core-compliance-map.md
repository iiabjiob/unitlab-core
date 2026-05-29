# IEC 61850 Report Core Compliance Map

Status: simulator-only core alignment map. This document is a guardrail for the future C# port and for the later backend MMS adapter. It does not claim hardware validation.

Normative local references:
- `/workspace/docs/.IEC61850/IEC 61850-6-2024.pdf`
- `/workspace/docs/.IEC61850/IEC 61850-7-2-2020.pdf`
- `/workspace/docs/.IEC61850/IEC 61850-8-1-2020.pdf`

## Boundary Rules

- IEC-facing names and semantics come from the local IEC 61850 PDFs.
- UnitLab DTOs are internal transfer models. They must stay JSON-serializable and portable to C# classes/enums.
- Vue, Pinia, DOM, browser APIs, backend clients, and renderer concepts must not enter `frontend/src/modules/iec61850-report-core`.
- Simulator behavior is validation scaffolding. It must not be documented or treated as a real IED/MMS guarantee.
- Real report session ownership will move to backend runtime before any real MMS adapter is enabled.

## Implemented Mapping

| Standard concept | Source | UnitLab core mapping | Status |
| --- | --- | --- | --- |
| `ReportControl` | IEC 61850-6-2024 | `SclReportControl`, `Iec61850ReportControlCandidate` | Implemented |
| `DataSet`, `FCDA`, `FCD` | IEC 61850-6-2024 | `SclDataSet`, `SclDataSetMember` | Implemented |
| `TrgOps` | IEC 61850-6-2024 / 7-2-2020 | `SclReportTriggerOptions`, `Iec61850ReportControlState.triggerOptions` | Implemented |
| `OptFields` | IEC 61850-6-2024 / 7-2-2020 | `SclReportOptionalFields`, `Iec61850ReportControlState.optionalFields` | Implemented |
| `RptEnabled`, `ClientLN` | IEC 61850-6-2024 | `SclReportEnabled`, `SclReportClient` | Inventory only |
| `RptID` | IEC 61850-7-2-2020 | `rptId` | Implemented |
| `RptEna` | IEC 61850-7-2-2020 | `enabled` | Simulator implemented, real write adapter-owned |
| `DatSet` | IEC 61850-7-2-2020 | `dataSetRef` | Implemented |
| `ConfRev` | IEC 61850-7-2-2020 | `confRev` | Implemented |
| `BufTm` | IEC 61850-7-2-2020 | `bufferTimeMs` | Implemented as state, future write adapter-owned |
| `IntgPd` | IEC 61850-7-2-2020 | `integrityPeriodMs` | Implemented as state, future write adapter-owned |
| `GI` | IEC 61850-7-2-2020 | `sendGeneralInterrogation()` | Simulator implemented |
| `SqNum` | IEC 61850-7-2-2020 | `sequenceNumber` | Implemented |
| `EntryID` | IEC 61850-7-2-2020 | `entryId` | Implemented in normalized event |
| `TimeOfEntry` | IEC 61850-7-2-2020 | `timeOfEntry` | Implemented in normalized event |
| `Owner` | IEC 61850-7-2-2020 | `owner` | Simulator implemented, real value adapter-owned |
| `Resv` | IEC 61850-7-2-2020 | `reservedBy` | Simulator implemented, real value adapter-owned |
| `ResvTms` | IEC 61850-7-2-2020 | none | Planned |
| `PurgeBuf` | IEC 61850-7-2-2020 | none | Planned |
| MMS report/data references | IEC 61850-8-1-2020 | `normalizeReportDataReference()` | Parser/normalizer support only; real MMS adapter planned |

## Trigger And Optional Fields

| Standard/SCL field | Internal field | Runtime reason or payload field |
| --- | --- | --- |
| `dchg` | `dataChange` | `data-change` |
| `qchg` | `qualityChange` | `quality-change` |
| `dupd` | `dataUpdate` | `data-update` |
| `period` | `periodic` | `integrity` |
| `gi` | `generalInterrogation` | `general-interrogation` |
| `seqNum` | `sequenceNumber` | `SqNum` |
| `timeStamp` | `timestamp` | `TimeOfEntry` or value timestamp |
| `reasonCode` | `reasonCode` | `ReasonForInclusion` as portable reason string |
| `dataSet` | `dataSetName` | `DataSet` / `dataSetRef` |
| `dataRef` | `dataReference` | `DataRef` |
| `entryID` | `entryId` | `EntryID` |
| `configRef` | `configRevision` | `ConfRev` |
| `bufOvfl` | `bufferOverflow` | `BufOvfl` |

## UnitLab-Only Terms

These are intentionally internal and must not be treated as IEC attributes:

| Internal term | Purpose |
| --- | --- |
| `lifecycleState` | Simulator/session diagnostic state |
| `Iec61850SimulatorEvent` | Simulator validation evidence |
| `Iec61850ReportRuntimeDiagnostic` | UnitLab validation result |
| `Iec61850ReportSubscriptionPlan` | FAT planning DTO derived from SCD and selected Signal List rows |

## C# Portability Rules

- Keep DTOs as records/classes with primitive fields, arrays/lists, and dictionaries only.
- Keep discriminated TypeScript unions small enough to map to C# enums.
- Keep `null` explicit for optional IEC fields.
- Do not rely on JavaScript object identity, closures, browser APIs, or reactive state.
- Keep standard term constants in sync with `standardTerms.ts`; this file is the human-readable map, while tests protect the runtime map.

## Open Gaps

- `DataTypeTemplates` are not expanded, so value type fidelity is shallow.
- `ResvTms` and `PurgeBuf` are not implemented.
- Indexed RCB instance allocation is not implemented.
- Real MMS encoding/decoding and service error mapping are not implemented.
- Backend persistence and report-to-test evidence linking are not implemented.
