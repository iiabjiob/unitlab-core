# IEC 61850 Report Runtime Plan

Status: slices 1-12, slice 13A-13U, and compliance guardrails started. Simulator-only report runtime contracts, the subscription plan builder, the simulator state machine, report event normalization, backend session ownership scaffolding, debug-view simulator execution, core subscription-plan execution, report-to-signal observation mapping, backend observation parity, backend subscription-plan execution, backend incoming report routing, activation precheck gating, the backend MMS endpoint boundary, the external IED simulator fixture boundary, the external simulator process scaffold, the fixture parser/model materialization, the external simulator model plan/loader boundary, model-plan blueprint validation, backend external simulator process preparation, backend external simulator lifecycle guardrails, backend external simulator process-plan orchestration, backend external simulator endpoint resolution, backend external simulator run orchestration, native external simulator loader validation, DataSet member path blueprinting, initial-value type preservation, report-control runtime blueprinting, report-control bit-mask blueprinting, DataSetEntry variable blueprinting, linked IED/LD/LN container creation, and the IEC/C# compliance map are implemented.

References:
- `docs/.IEC61850/IEC 61850-6-2024.pdf` for SCL source structure.
- `docs/.IEC61850/IEC 61850-7-2-2020.pdf` for DataSet, report control, trigger option, optional field, reservation, enable, and GI semantics.
- `docs/.IEC61850/IEC 61850-8-1-2020.pdf` for the future MMS mapping.
- `docs/architecture/iec61850-report-core-compliance-map.md` for the current IEC-to-UnitLab mapping and C# portability guardrails.
- `docs/architecture/iec61850-self-owned-mms-client-plan.md` for the self-owned MMS client decision, required standards, public references, and implementation slices.

## Safety Position

No real IED connection is allowed in the first runtime slices. All connect/read/reserve/enable/disable/GI behavior must run through a simulator adapter until the simulator flows, diagnostics, and operator-visible state are proven.

The production owner for live device communication remains backend/runtime. Frontend/core modules may define portable contracts and simulator behavior, but they must not become the authority for real MMS sessions, report enablement, runtime evidence, or hardware-facing state.

GOOSE and Sampled Values are explicitly out of scope for this plan and stay in a later protocol slice.

## Target Runtime Flow

1. Build SCD report inventory from `NormalizedSclModel.reportSubscriptions`.
2. Merge the operator-selected Signal List rows with SCD report signals.
3. Build a report subscription plan containing the required IEDs, RCBs, DataSets, and signal references.
4. Connect to an endpoint through an adapter.
5. Read live RCB state: `DatSet`, `ConfRev`, `TrgOps`, `OptFlds`, reservation/owner state, `RptEna`, buffer/integrity settings.
6. Compare live state with the SCD-derived plan and block reservation/enable when the comparison returns error diagnostics.
7. Reserve the selected RCB instance when required.
8. Configure only allowed fields while disabled.
9. Enable the report.
10. Send GI and normalize the resulting report event.
11. Process incoming reports into signal-level observations.
12. Disable and release the RCB during cleanup.

## Slice Plan

### Slice 1 - Simulator Contracts And Read Manager

Implemented in this slice:

- Framework-neutral report runtime contracts in `frontend/src/modules/iec61850-report-core`.
- `Iec61850ReportManager` with adapter-owned connection lifecycle.
- Simulator adapter for read, reserve, release, enable, disable, and GI behavior.
- SCD/live comparison diagnostics for DataSet reference, `ConfRev`, signal count, `TrgOps`, and `OptFlds`.
- `Iec61850ReportSubscriptionCandidate` now carries `bufferTimeMs`, `integrityPeriodMs`, `triggerOptions`, and `optionalFields`.

Validation:

- Unit tests run the full simulator flow without real devices.
- No backend, MMS, GOOSE, SV, or hardware communication is introduced.

### Slice 2 - Subscription Plan Builder

Implemented in this slice:

- Build a deterministic subscription plan from SCD report candidates plus selected Signal List rows.
- Group by IED/access point/logical device/report control.
- Prefer exact full-path signal matches.
- Mark unresolved, duplicate, and ambiguous signal matches.
- Distinguish required reports from candidate reports.
- Support parent FCD matches for selected leaf addresses when the DataSet member is a parent object.
- Keep ST/CO/MX functional constraints distinct when the selected address includes an explicit functional constraint.

Validation:

- Unit tests cover duplicate selections, unresolved signals, ambiguous signals across IEDs, parent FCD matches, and multiple report candidates for one selected signal.

### Slice 3 - Simulator State Machine Hardening

Implemented in this slice:

- Explicit lifecycle states: disconnected, connected, read, reserved, enabled, GI pending, reporting, disabled, released, failed.
- Deterministic failures for reservation conflict, enable without reservation, GI while disabled, stale `ConfRev`, and disconnect during enabled state.
- Simulator event log for command/read/report operations.
- `Iec61850ReportControlState.lifecycleState` now exposes the last simulator report-control lifecycle state.
- `createIec61850SimulatorAdapter()` returns an adapter with `getEventLog()` and `clearEventLog()` for simulator validation.
- Enabled disconnect failure is available through `strictDisconnectWhileEnabled` so existing short-lived manager calls remain compatible while strict session behavior can be tested.

Validation:

- State transition tests.
- Failure and cleanup tests.

### Slice 4 - Report Event Normalization

Implemented in this slice:

- Normalize report payloads into `Iec61850ReportEvent`.
- Map values to DataSet order.
- Carry sequence number, time of entry, reason code, DataSet reference, config revision, entry ID, buffer overflow, and optional data references when present.
- Surface missing optional fields as diagnostics, not fatal errors.
- Preserve reported subset events for data change, quality change, and integrity reports while still ordering known values by SCD DataSet index.
- Support common report data-reference forms used by simulator/MMS-facing adapters: SCD dot form, slash form, IED-prefixed full path, and `$FC$` MMS-style form.
- Route simulator GI through the same normalizer used for future adapter payloads.

Validation:

- Simulator report fixtures for GI, data change, quality change, integrity, duplicate sequence number, and `ConfRev` mismatch.

### Slice 5 - Backend Runtime Boundary

Implemented in this slice:

- Before this slice starts, keep `standardTerms.ts` and the compliance map aligned with the IEC-facing DTOs.
- Internal backend runtime service boundary in `backend/app/services/iec61850`.
- Backend-owned simulator session lifecycle: open, read, reserve, enable, GI, disable, release, close.
- Simulator event log for command/read/report evidence.
- Deterministic backend errors for missing sessions, enable without reservation, GI while disabled, reservation conflict, and strict enabled disconnect.
- No public REST/WebSocket contract is added in this slice.

Still planned:

- Move live session ownership to backend services before any real MMS adapter exists.
- REST remains for explicit plan/read/prepare commands.
- WebSocket emits runtime state and normalized report events.
- Persist only auditable runtime actions and later test evidence; do not persist transient frontend session state as authority.

Validation:

- Backend service tests using simulator adapter only.
- No real network device tests in this slice.

### Slice 6 - Debug UI Integration

Implemented in this slice:

- The 61850 debug view can run an explicit simulator-only GI cycle after Signal List merge.
- The debug view shows the simulator plan summary, report lifecycle result, simulator event log, diagnostics, and GI value count.
- The UI action remains explicit; selecting tree nodes, Signal List rows, reports, or DataSets does not reserve, enable, disable, or run GI.
- The implementation uses the frontend simulator adapter only and does not add public REST/WebSocket contracts or real MMS/device communication.

Still planned:

- Backend REST/WebSocket runtime UI integration once the public API shape is approved.
- Real-device mode must remain visually distinct from simulator mode and must stay backend-owned.
- Browser verification with large project SCD files.

Validation:

- Frontend unit tests cover explicit simulator plan/GI execution from debug-view merge data.
- No implicit reserve/enable from selection alone.

### Slice 7 - Core Subscription Plan Runner

Implemented in this slice:

- Subscription-plan execution moved into `frontend/src/modules/iec61850-report-core`.
- The runner executes read, reserve, enable, GI, disable, and release for each required report in the plan.
- The runner always attempts cleanup after partial failure; a reserved report is released even when activation fails.
- The debug view now calls the core simulator runner instead of owning runtime sequencing itself.
- Simulator endpoint/device construction is kept in the portable core module for later backend/C# parity work.

Validation:

- Frontend unit tests cover successful GI execution and cleanup after simulator activation failure.
- No real MMS/device communication, public REST endpoint, or WebSocket contract is introduced.

### Slice 8 - Report Event Observation Mapping

Implemented in this slice:

- Normalized report events can be mapped back to selected Signal List rows through the subscription plan.
- `Iec61850SignalObservation` carries selected signal id/address/label, model reference, report value, reason code, timestamp, and match kind.
- Subset reports, such as data-change events, produce observations only for included values and info diagnostics for selected signals not included in that event.
- Unplanned report events are surfaced as observation diagnostics instead of being silently dropped.
- The core subscription runner now includes signal observations for each successful report event.

Validation:

- Frontend unit tests cover GI observations, subset data-change observations, and unplanned report diagnostics.
- Observations are UnitLab evidence DTOs, not IEC attributes.

### Slice 9 - Backend Observation Mapping Parity

Implemented in this slice:

- Backend simulator runtime now has signal observation DTOs mirroring the portable frontend core shape.
- Backend report events can be mapped to selected Signal List observations with selected signal id/address/label, model reference, report value, reason code, timestamp, and match kind.
- Backend mapper supports MMS-style `$FC$` data references, slash references, IED-prefixed references, and SCD dot references.
- Subset report events produce observations for included selected signals and info diagnostics for selected signals not included in that event.
- No public REST/WebSocket API is added.

Validation:

- Backend service tests cover GI observation mapping and subset data-change observation diagnostics.
- Simulator-only; no real MMS/device communication.

### Slice 10 - Backend Subscription Plan Runner

Implemented in this slice:

- Backend runtime can execute a full subscription plan grouped by IED/access point.
- Each required report follows the simulator-backed sequence: connect, read, reserve, enable, GI, observation mapping, disable, release, disconnect.
- Partial failures still attempt cleanup; a report reserved before activation failure is released before the run result is returned.
- Plan run results carry per-report runtime status, normalized report event, selected signal observations, diagnostics, and error code/message.
- Simulator wrapper returns the backend simulator event log as validation evidence.
- No public REST/WebSocket API, real MMS connection, GOOSE, or SV handling is added.

Validation:

- Backend service tests cover successful simulator plan execution with observations.
- Backend service tests cover cleanup/release after activation failure.
- Simulator-only; no real MMS/device communication.

### Slice 11 - Backend Incoming Report Event Routing

Implemented in this slice:

- Backend runtime can route a normalized incoming report event through a subscription plan before producing signal observations.
- Report events matching required plan reports reuse the same selected-signal observation mapper used by GI execution.
- Report events that do not match any planned ReportControl produce `REPORT_NOT_IN_PLAN` diagnostics and preserve reported values as unselected evidence.
- The mapper is simulator-safe and adapter-neutral; it does not open sockets, add REST/WebSocket contracts, or enable real MMS.

Validation:

- Backend service tests cover planned subset data-change report routing.
- Backend service tests cover unplanned report event diagnostics and unselected value preservation.
- Simulator-only; no real MMS/device communication.

### Slice 12 - Activation Precheck Gate

Implemented in this slice:

- Core and backend subscription runners stop after the read stage when live ReportControl state has error diagnostics.
- Blocking precheck failures return `REPORT_CONTROL_PRECHECK_FAILED` with the read diagnostics preserved as run evidence.
- Reservation, enable, GI, disable, and release are not attempted when the read comparison already proves the live RCB does not match the SCD-derived plan.
- Warning diagnostics remain non-blocking so `TrgOps`/`OptFlds` differences can still be surfaced without preventing simulator validation.
- No public REST/WebSocket API, real MMS connection, GOOSE, or SV handling is added.

Validation:

- Frontend core tests cover simulator precheck failure without reserve/enable.
- Backend service tests cover simulator/backend precheck failure without reserve/enable.
- Simulator-only; no real MMS/device communication.

### Slice 13 - MMS Adapter Spike Behind Simulator Parity

Planned only after simulator runner parity passes:

- Add an adapter boundary for IEC 61850-8-1 MMS.
- Connect/read RCB attributes against a controlled simulator or lab IED.
- Keep the adapter replaceable: reference-stack adapters and the future self-owned MMS client must expose the same UnitLab report-runtime contract.
- Keep real enable/reserve/GI behind a backend feature flag and explicit operator action.
- Do not embed GPL MMS code in UnitLab production runtime without a separate licensing decision.

Validation:

- Lab-only validation checklist.
- Timeout, disconnect, reservation conflict, stale state, and cleanup evidence.

### Slice 13A - Backend MMS Endpoint Boundary

Implemented in this slice:

- Backend runtime can map a planned IED/access point to an explicit MMS endpoint catalog entry.
- The endpoint catalog fails closed for missing, duplicate, empty-host, and invalid-port entries.
- An unavailable MMS adapter exists only as a guardrail; it accepts MMS endpoints and returns `MMS_ADAPTER_NOT_IMPLEMENTED` instead of simulating success.
- The existing subscription-plan runner can now exercise the real-MMS boundary shape without opening a socket or embedding a third-party stack.

Still planned:

- External libIEC61850 IED simulator process for internal tests.
- Real MMS client adapter or self-owned MMS client implementation behind the same runtime contract.

Validation:

- Backend service tests cover endpoint resolution, missing endpoint diagnostics, and fail-closed MMS adapter behavior.

### Slice 13B - External IED Simulator Fixture Boundary

Implemented in this slice:

- Backend runtime can export a required-report subset of a subscription plan as a JSON-serializable IED simulator fixture.
- The fixture contains only IED/access point, DataSet members, ReportControl attributes, trigger options, and optional fields needed by an external MMS IED simulator.
- Signal List selected-signal IDs and FAT test semantics are intentionally excluded from the fixture; UnitLab still owns matching and evidence.
- Fixture export fails closed when a required ReportControl has no `DatSet` reference.

Still planned:

- C/libIEC61850 process that consumes this fixture and exposes one simulated IED over MMS.
- Integration test that connects UnitLab backend to that process through the MMS endpoint catalog.

Validation:

- Backend service tests cover fixture payload shape, required-report filtering, selected-signal isolation, and missing-DataSet rejection.

### Slice 13C - External IED Simulator Process Scaffold

Implemented in this slice:

- Added `simulator/iec61850_ied` as the isolated home for the future libIEC61850-based IED simulator process.
- Added a C/CMake CLI scaffold that accepts `--fixture`, `--ied`, `--bind`, `--port`, and `--dry-run`.
- The scaffold validates the UnitLab fixture schema and selected IED name in dry-run mode.
- Non-dry-run execution fails closed with `MMS_SERVER_NOT_IMPLEMENTED`; it does not pretend to expose MMS.
- Optional CMake wiring can check libIEC61850 headers/library without making UnitLab backend depend on them.

Still planned:

- Fixture JSON parser and libIEC61850 server model loader.
- One DataSet plus one URCB/BRCB exposed over MMS.
- Backend integration test connecting through the MMS endpoint catalog.

Validation:

- Native C compile and dry-run fixture smoke test.

### Slice 13D - External IED Simulator Fixture Parser

Implemented in this slice:

- The external simulator scaffold now has a no-dependency fixture scanner for the UnitLab fixture schema.
- Dry-run mode validates exact schema, selected IED name, access point, DataSet array, report array, and DataSet member count.
- The scanner reports fixture counts for the selected IED so the future libIEC61850 model loader has a deterministic input summary.
- It remains schema-specific and is not a general JSON parser.

Still planned:

- Materialize parsed DataSet/report records into model-loader structures.
- Create the libIEC61850 IED model from those records.

Validation:

- Native C compile, positive dry-run fixture smoke test, and negative missing-IED dry-run smoke test.

### Slice 13E - External IED Simulator Fixture Model Records

Implemented in this slice:

- The fixture scanner now materializes the selected IED into C records for access point, DataSets, DataSet members, and ReportControls.
- Dry-run output includes first DataSet, first signal, and first ReportControl keys for deterministic model-loader smoke checks.
- Parsed records are heap-owned by the simulator process and released through an explicit free function.
- The parser still remains limited to UnitLab fixture schema and does not become a general JSON dependency.

Still planned:

- Parse trigger option and optional-field subobjects into C records.
- Feed the materialized records into libIEC61850 model creation.

Validation:

- Native C compile, positive dry-run fixture smoke test, and negative missing-IED dry-run smoke test.

### Slice 13F - External IED Simulator Report Option Records

Implemented in this slice:

- The simulator fixture parser now materializes `triggerOptions` and `optionalFields` into typed C records.
- Dry-run output surfaces first-report GI trigger and data-reference optional-field values for deterministic smoke checks.
- Missing `triggerOptions` or `optionalFields` now fails fixture parsing before any future MMS server can start.

Still planned:

- Feed the report option records into libIEC61850 report-control creation.
- Add fixture-level negative tests for malformed option values once a simulator test harness exists.

Validation:

- Native C compile, CMake configure/build, and positive dry-run fixture smoke test.

### Slice 13G - External IED Simulator Model Plan

Implemented in this slice:

- Added a C model-plan builder that derives logical devices, logical nodes, DataSets, and ReportControls from the materialized fixture.
- The model plan validates that every ReportControl references an exported DataSet.
- DataSet member references are parsed into logical-device/logical-node ownership before any future MMS server starts.
- Dry-run output now reports model LD/LN/DataSet/Report counts and first LD/LN keys.

Still planned:

- Convert the model plan into libIEC61850 `IedModel`, DataSet, and ReportControl objects.
- Add C-level negative fixtures for invalid signal references and missing report DataSets.

Validation:

- Native C compile, CMake configure/build, and positive dry-run fixture smoke test.

### Slice 13H - External IED Simulator Loader Boundary

Implemented in this slice:

- Added a fail-closed model-loader boundary above the external simulator model plan.
- Non-dry-run execution validates the fixture, builds the model plan, and then fails with `LIBIEC61850_NOT_LINKED` unless the simulator is built with libIEC61850.
- When compiled with libIEC61850 support, the boundary still fails with `LIBIEC61850_MODEL_LOADER_NOT_IMPLEMENTED` until the real model creation slice lands.
- No MMS server is started and no fake success is reported.

Still planned:

- Create libIEC61850 model objects from the model plan.
- Start one MMS server only after the model load path is real.

Validation:

- Native C compile, CMake configure/build, dry-run smoke test, and fail-closed non-dry-run smoke test.

### Slice 13I - External IED Simulator Model Blueprint

Implemented in this slice:

- The external simulator model plan now carries normalized blueprint records for DataSets, ReportControls, and DataSet member signals.
- DataSet references are validated against the selected IED/access point before any future MMS server startup.
- DataSet member references are normalized into logical device, logical node, object reference, functional constraint, member index, and initial value records.
- Signal functional constraint mismatches between the reference suffix and fixture `fc` field fail closed.
- Added a native C model-plan test target for positive blueprint construction and negative malformed plan cases.

Still planned:

- Convert the blueprint records into libIEC61850 `IedModel`, DataSet, ReportControl, and value objects.
- Start one MMS server only after the model load path is real.

Validation:

- Native C compile, CMake configure/build, CTest model-plan test, dry-run smoke test, and fail-closed non-dry-run smoke test.

### Slice 13J - Backend External Simulator Process Boundary

Implemented in this slice:

- Backend runtime can write an external IED simulator fixture file from the existing fixture DTO.
- Backend runtime can build a safe no-shell process command for the external simulator binary with fixture path, selected IED, bind address, and port.
- The process spec exposes the MMS endpoint that UnitLab will use once the external simulator runs a real MMS server.
- The startup check runs the simulator in `--dry-run` mode first and fails closed on missing binary, missing fixture device, timeout, or non-zero exit.
- No public REST/WebSocket API and no real MMS connection are introduced.

Still planned:

- Start and supervise a long-running simulator process after the C simulator can expose real MMS.
- Connect backend report-runtime execution to that endpoint through the MMS adapter boundary.

Validation:

- Backend service tests cover fixture-file writing, command construction, endpoint shape, missing binary, missing IED, safe process invocation, and failed dry-run checks.

### Slice 13K - Backend External Simulator Process Lifecycle

Implemented in this slice:

- Backend runtime can start the external simulator process only from a non-dry-run process spec.
- Process start always runs the existing dry-run startup check before spawning the long-running command.
- The long-running process is spawned without shell expansion and with captured stdout/stderr.
- Premature process exit during the startup grace period fails closed with simulator stderr/stdout included in the diagnostic.
- Backend runtime can stop the simulator process with terminate first and kill fallback after timeout.
- No public REST/WebSocket API and no real MMS connection are introduced.

Still planned:

- Use this lifecycle helper only after the C simulator can expose a real MMS server.
- Connect backend report-runtime execution to that endpoint through the MMS adapter boundary.

Validation:

- Backend service tests cover no-shell spawn, dry-run-spec rejection, premature-exit diagnostics, and terminate/kill cleanup.

### Slice 13L - Backend External Simulator Process Plan

Implemented in this slice:

- Backend runtime can write one fixture file for a required-device set and build deterministic simulator process specs for each fixture device.
- Process specs receive deterministic ports from a caller-provided base port.
- The process plan exposes the MMS endpoints UnitLab will later use for report-runtime execution.
- Backend runtime can run dry-run startup checks across the full process plan.
- Backend runtime can start process-plan specs sequentially and stops already-started simulators if a later simulator fails during startup.
- No public REST/WebSocket API and no real MMS connection are introduced.

Still planned:

- Use process-plan start only after the C simulator exposes a real MMS server.
- Connect backend report-runtime execution to those endpoints through the MMS adapter boundary.

Validation:

- Backend service tests cover process-plan fixture writing, deterministic spec/endpoint generation, dry-run checks, and cleanup after partial start failure.

### Slice 13M - Backend External Simulator Endpoint Resolution

Implemented in this slice:

- Backend runtime can prepare an external simulator process plan directly from a report subscription plan.
- The process plan resolves `Iec61850ReportSubscriptionPlanDevice` to the MMS endpoint for that simulated IED/access point.
- Missing and duplicate simulator endpoints fail closed before report-runtime execution.
- The resolved endpoints can feed the existing report-runtime runner through the same MMS adapter boundary; with the unavailable MMS adapter this still returns `MMS_ADAPTER_NOT_IMPLEMENTED`.
- No public REST/WebSocket API and no real MMS connection are introduced.

Still planned:

- Use endpoint resolution with a real MMS adapter only after the C simulator exposes MMS and the adapter can read live RCB state.

Validation:

- Backend service tests cover direct subscription-plan preparation, endpoint resolution, missing/duplicate endpoint failures, and fail-closed MMS adapter execution through process-plan endpoints.

### Slice 13N - Backend External Simulator Run Orchestration

Implemented in this slice:

- Backend runtime can prepare the external simulator process plan, start the simulator processes, run the existing report subscription runner through resolved MMS endpoints, and stop all started processes.
- Simulator processes are stopped even when the report-runtime adapter fails.
- The wrapper returns the process plan, subscription-run result, and process stop results for backend evidence plumbing.
- With the unavailable MMS adapter, execution still fails closed with `MMS_ADAPTER_NOT_IMPLEMENTED`.
- No public REST/WebSocket API and no real MMS connection are introduced.

Still planned:

- Use this wrapper with a real MMS adapter only after the C simulator exposes MMS and the adapter can read live RCB state.

Validation:

- Backend service tests cover fail-closed MMS adapter execution through external simulator processes and cleanup after runtime exceptions.

### Slice 13O - External IED Simulator Loader Validation

Implemented in this slice:

- Added a native C test target for the external simulator model-loader boundary.
- The loader is validated to fail closed when libIEC61850 is not linked, and to report the not-implemented loader code when compiled with libIEC61850 support.
- Guardrails cover null arguments, empty model plans, empty bind addresses, and invalid TCP ports before any future MMS server can start.
- CMake/CTest now runs both the model-plan blueprint test and the model-loader guardrail test.
- No MMS server is started and no fake report success is reported.

Still planned:

- Convert the validated model plan into real libIEC61850 server model objects.
- Start one MMS server only after the model load path is real.

Validation:

- Native C compile, CMake configure/build, and CTest model-plan/model-loader tests.

### Slice 13P - External IED Simulator DataSet Member Path Blueprint

Implemented in this slice:

- The external simulator model plan now keeps the original DataSet member kind (`FCD` or `FCDA`) beside each normalized signal.
- Signal object references are split into a data object name and optional data attribute path before the future libIEC61850 loader runs.
- Unsupported DataSet member kinds fail closed during model-plan construction.
- Dry-run output exposes the first model signal kind, data object, and data attribute path for deterministic fixture checks.
- No MMS server is started and no fake report success is reported.

Still planned:

- Convert the validated DO/DA path blueprint into real libIEC61850 data objects and DataAttributes.
- Start one MMS server only after the model load path is real.

Validation:

- Native C compile, CMake configure/build, and CTest model-plan/model-loader tests.

### Slice 13Q - External IED Simulator Initial Value Types

Implemented in this slice:

- The external simulator fixture parser preserves the JSON type of each DataSet member `initialValue`.
- Supported initial value kinds are null, boolean, integer, real, and string.
- The model plan carries the initial value kind beside the raw value before the future libIEC61850 loader creates MMS values.
- Unknown or malformed initial value tokens fail closed during fixture parsing or model-plan construction.
- Dry-run output exposes the first model signal value kind and raw initial value.
- No MMS server is started and no fake report success is reported.

Still planned:

- Map typed initial values into real libIEC61850/MMS values once model creation is implemented.
- Replace fixture defaults with values derived from SCL DataTypeTemplates or explicit simulator test fixtures where available.

Validation:

- Native C compile, CMake configure/build, CTest fixture-parser/model-plan/model-loader tests, and dry-run fixture smoke test.

### Slice 13R - External IED Simulator ReportControl Runtime Blueprint

Implemented in this slice:

- The external simulator model plan now carries ReportControl runtime attributes required by future `ReportControlBlock_create` calls.
- The blueprint preserves `RptID`, buffered/unbuffered kind, `ConfRev`, indexed flag, `BufTm`, `IntgPd`, trigger options, and optional fields.
- Unsupported report kinds and malformed `ConfRev` values fail closed during model-plan construction.
- Dry-run output exposes the first model ReportControl runtime fields for deterministic fixture checks.
- No MMS server is started and no fake report success is reported.

Still planned:

- Map trigger options and optional fields to the libIEC61850 RCB bit masks.
- Create real libIEC61850 URCB/BRCB instances from this blueprint.

Validation:

- Native C compile, CMake configure/build, CTest fixture-parser/model-plan/model-loader tests, and dry-run fixture smoke test.

### Slice 13S - External IED Simulator ReportControl Bit Masks

Implemented in this slice:

- The external simulator model plan now converts fixture `TrgOps` into the libIEC61850-compatible trigger option bit mask.
- The external simulator model plan now converts fixture `OptFlds` into the libIEC61850-compatible report option bit mask.
- The bit constants are kept in the simulator model-plan boundary so the future loader can call `ReportControlBlock_create` without rereading fixture DTOs.
- Dry-run output exposes the first model ReportControl `TrgOps` and `OptFlds` masks for deterministic fixture checks.
- No MMS server is started and no fake report success is reported.

Still planned:

- Create real libIEC61850 URCB/BRCB instances from the ReportControl blueprint and masks.
- Start one MMS server only after model creation succeeds.

Validation:

- Native C compile, CMake configure/build, CTest fixture-parser/model-plan/model-loader tests, and dry-run fixture smoke test.

### Slice 13T - External IED Simulator DataSetEntry Variable Blueprint

Implemented in this slice:

- The external simulator model plan now converts each DataSet member object reference into the MMS variable-name format expected by libIEC61850 `DataSetEntry_create`.
- DataSetEntry variable names include LD, LN, FC, and `$`-separated object path components.
- The blueprint keeps an explicit optional component field for future array-member support, but current fixture members use no component.
- Dry-run output exposes the first model DataSetEntry variable and component for deterministic fixture checks.
- No MMS server is started and no fake report success is reported.

Still planned:

- Use the DataSetEntry variable blueprint when creating real libIEC61850 DataSets.
- Add array index/component support only when a fixture or SCD source proves it is needed.

Validation:

- Native C compile, CMake configure/build, CTest fixture-parser/model-plan/model-loader tests, and dry-run fixture smoke test.

### Slice 13U - External IED Simulator Linked Container Creation

Implemented in this slice:

- When built with `UNITLAB_IEC61850_SIM_WITH_LIBIEC61850=ON`, the external simulator loader now creates and destroys a dynamic libIEC61850 `IedModel`.
- The linked loader creates logical devices and logical nodes from the validated model plan before failing closed.
- The linked loader now fails with `LIBIEC61850_DO_DA_LOADER_NOT_IMPLEMENTED` instead of the earlier generic not-implemented code.
- The model-loader CTest target also links against libIEC61850 when the simulator build option is enabled.
- No data objects, data attributes, DataSets, ReportControls, MMS server, or fake report success are created in this slice.

Still planned:

- Create data objects and data attributes from the DataSet member blueprint.
- Create real libIEC61850 DataSets and URCB/BRCB instances after DO/DA creation works.

Validation:

- Native C compile, CMake configure/build, CTest fixture-parser/model-plan/model-loader tests, and dry-run fixture smoke test without libIEC61850 linked.

### Slice 13V - External IED Simulator Dynamic DataSet/RCB Creation

Implemented in this slice:

- When built with `UNITLAB_IEC61850_SIM_WITH_LIBIEC61850=ON`, the external simulator loader now consumes the validated model plan through real libIEC61850 dynamic model APIs.
- The linked loader creates data objects, FCDA data attribute paths, initial MMS values, DataSets, DataSet entries, and ReportControls after LD/LN creation succeeds.
- The linked loader now fails closed with `LIBIEC61850_SERVER_NOT_IMPLEMENTED` only after the dynamic model, DataSets, and ReportControls have been created and destroyed.
- The unlinked path still fails closed with `LIBIEC61850_NOT_LINKED`.
- No MMS server is started and no fake report success is reported.

Still planned:

- Start one local `IedServer` from the dynamic model.
- Add a local client smoke test that connects to the simulator and reads DataSet/RCB metadata.
- Expand typed SCD/DataTypeTemplates support before claiming accurate FCD/CDC expansion.

Validation:

- Native C compile, CMake configure/build, CTest fixture-parser/model-plan/model-loader tests without libIEC61850 linked.
- Linked CMake configure/build and CTest fixture-parser/model-plan/model-loader tests against a locally built libIEC61850 checkout.
- Linked non-dry-run smoke test verifies the model creation path reaches the explicit server-not-implemented boundary.

### Slice 13W - External IED Simulator MMS Server Runtime

Implemented in this slice:

- When built with `UNITLAB_IEC61850_SIM_WITH_LIBIEC61850=ON`, the external simulator can now create an `IedServer` from the dynamic model and start listening on the configured TCP endpoint.
- Non-dry-run simulator execution keeps the process alive until SIGTERM or SIGINT, then stops and destroys the libIEC61850 server and dynamic model cleanly.
- `--smoke-start` starts and immediately stops the linked MMS server for deterministic local validation without leaving a process running.
- The unlinked path still fails closed with `LIBIEC61850_NOT_LINKED`.
- No fake report subscription success is reported; backend MMS adapter support remains a separate slice.

Still planned:

- Add a local libIEC61850 client smoke check that connects to the simulator and reads DataSet/RCB metadata.
- Wire the backend MMS adapter to the external simulator endpoint.
- Add explicit process readiness probing instead of process-liveness-only startup checks.

Validation:

- Native C compile, CMake configure/build, CTest fixture-parser/model-plan/model-loader tests without libIEC61850 linked.
- Linked CMake configure/build and CTest fixture-parser/model-plan/model-loader tests against a locally built libIEC61850 checkout.
- Linked `--smoke-start` validates server start/stop.
- Linked non-dry-run run under SIGTERM validates process lifetime and clean shutdown.

## Current Risks

- The SCD parser does not expand `DataTypeTemplates`; value typing remains shallow.
- RCB indexed instance allocation is not planned yet.
- Production MMS client transport is unresolved; the current direction is a replaceable backend adapter, with a self-owned MMS client tracked separately.
- Report event persistence and test evidence linking are future backend slices.
- GOOSE and SV remain separate protocol tracks.
