# IEC 61850 Report Runtime Plan

Status: slices 1-12 plus compliance guardrails started. Simulator-only report runtime contracts, the subscription plan builder, the simulator state machine, report event normalization, backend session ownership scaffolding, debug-view simulator execution, core subscription-plan execution, report-to-signal observation mapping, backend observation parity, backend subscription-plan execution, backend incoming report routing, activation precheck gating, and the IEC/C# compliance map are implemented.

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

## Current Risks

- The SCD parser does not expand `DataTypeTemplates`; value typing remains shallow.
- RCB indexed instance allocation is not planned yet.
- Real MMS transport is unresolved; the current direction is a replaceable backend adapter, with a self-owned MMS client tracked separately.
- Report event persistence and test evidence linking are future backend slices.
- GOOSE and SV remain separate protocol tracks.
