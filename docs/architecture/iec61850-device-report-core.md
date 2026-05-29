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

## Core Boundaries

`frontend/src/modules/scd-sld-core` is framework-neutral and remains the portable boundary for a future C# migration.

Current file responsibilities:

- `xmlScanner.ts`: lightweight SCL tag/attribute scanner. It is not a full XML parser.
- `parser.ts`: orchestration only; builds `NormalizedSclModel` and runs normalization steps.
- `topologyParser.ts`: SCD topology extraction for `Substation`, `VoltageLevel`, `Bay`, equipment, terminals, connectivity nodes, and `LNode` references.
- `communicationParser.ts`: IED/access point/server/logical device/logical node/DataSet/ReportControl inventory extraction and report DataSet resolution.
- `topologyNormalizer.ts`: terminal-to-connectivity-node normalization and unresolved topology diagnostics.
- `parserUtils.ts`: shared parser utilities for stable IDs, primitive attribute conversion, path normalization, and diagnostic merging.
- `graph.ts`: electrical graph construction from normalized topology.
- `cellModel.ts`: renderer-neutral SLD cell grouping and bay interpretation.
- `layout.ts`: deterministic SLD layout and routing.
- `sldDocumentBase.ts`: internal graph-to-SLD DTO mapping shared by production layout and debug wrappers.
- `sldDocument.ts`: flat/debug document helpers only; production generation uses `generateSldFromScd`.

## Debug Visualization

`frontend/src/pages/debug61850/Iec61850DebugPage.vue` visualizes the normalized SCL model through the existing Affino Treeview panel and property panel.

The tree now shows:

- electrical topology from `Substation` to switchgear equipment;
- IED access points, servers, logical devices, and logical nodes;
- DataSets and their signal members;
- ReportControls and the resolved report signal set.

This view is inspection-only. Loading an SCD file in the debug page does not create report subscriptions, enable reports, reserve RCBs, or persist runtime evidence.

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
