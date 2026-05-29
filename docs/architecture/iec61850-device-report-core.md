# IEC 61850 Device Report Core

Status: initial core slice implemented in `frontend/src/modules/scd-sld-core`.

Reference standards:
- `docs/.IEC61850/IEC 61850-6-2024.pdf` for SCL structure.
- `docs/.IEC61850/IEC 61850-7-2-2020.pdf` for DataSet and report control semantics.
- `docs/.IEC61850/IEC 61850-8-1-2020.pdf` for future MMS report mapping.

## Implemented Behavior

The SCD core now extracts a runtime-facing IEC 61850 inventory alongside the existing SLD topology model:

- `IED`
- `AccessPoint`
- `Server`
- `LDevice`
- `LN0` / `LN`
- `DataSet`
- `FCDA` / `FCD` DataSet members
- `ReportControl`
- `TrgOps`
- `OptFields`
- `RptEnabled` / `ClientLN`

`NormalizedSclModel.reportSubscriptions` is a derived inventory for future report subscription work. It links each `ReportControl` to the resolved DataSet when the DataSet is found in the same logical device scope, and carries the DataSet member references that will become report signal bindings.

## Runtime Boundary

This slice does not subscribe to devices, open MMS sessions, write backend state, or mutate hardware-facing runtime state. It only normalizes SCD metadata so the later backend/runtime slice can validate report subscriptions against an explicit device and DataSet structure.

The future runtime owner must remain backend-side. The frontend debug view may visualize this model, but it must not become the source of truth for report enablement, received report ordering, acknowledgements, stale state, or evidence persistence.

## Known Gaps

- No MMS connection, report enable, reservation, GI, or buffered entry replay is implemented.
- DataTypeTemplates are not expanded into full DO/DA typed value trees yet.
- DataSet member references are normalized from SCL attributes only; no live IED model discovery is performed.
- Ambiguous or missing ReportControl-to-DataSet links are surfaced as parser diagnostics and are not auto-guessed.

## Validation

Focused validation should cover:

- SCD parsing of IED access points, logical devices, logical nodes, DataSets, and report controls.
- ReportControl DataSet resolution in same-LN and same-logical-device scopes.
- Missing DataSet diagnostics.
- Future backend slices must add simulator or mock MMS validation before any hardware-facing report subscription is exposed.
