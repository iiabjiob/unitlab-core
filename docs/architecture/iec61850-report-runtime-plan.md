# IEC 61850 Report Runtime Plan

Status: slice 1 started. Simulator-only report runtime contracts are implemented in `frontend/src/modules/iec61850-report-core`.

References:
- `docs/.IEC61850/IEC 61850-6-2024.pdf` for SCL source structure.
- `docs/.IEC61850/IEC 61850-7-2-2020.pdf` for DataSet, report control, trigger option, optional field, reservation, enable, and GI semantics.
- `docs/.IEC61850/IEC 61850-8-1-2020.pdf` for the future MMS mapping.

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
6. Compare live state with the SCD-derived plan and surface diagnostics.
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

Planned:

- Build a deterministic subscription plan from SCD report candidates plus selected Signal List rows.
- Group by IED/access point/logical device/report control.
- Prefer exact full-path signal matches.
- Mark unresolved, duplicate, and ambiguous signal matches.
- Distinguish required reports from candidate reports.

Validation:

- Fixtures with duplicate device names, repeated DataSet members, missing DataSets, and mixed ST/CO/MX functional constraints.

### Slice 3 - Simulator State Machine Hardening

Planned:

- Explicit lifecycle states: disconnected, connected, read, reserved, enabled, GI pending, reporting, disabled, released, failed.
- Deterministic failures for reservation conflict, enable without reservation, GI while disabled, stale `ConfRev`, and disconnect during enabled state.
- Simulator event log for command/read/report operations.

Validation:

- State transition tests.
- Failure and cleanup tests.

### Slice 4 - Report Event Normalization

Planned:

- Normalize report payloads into `Iec61850ReportEvent`.
- Map values to DataSet order.
- Carry sequence number, time of entry, reason code, DataSet reference, config revision, entry ID, buffer overflow, and optional data references when present.
- Surface missing optional fields as diagnostics, not fatal errors.

Validation:

- Simulator report fixtures for GI, data change, quality change, integrity, duplicate sequence number, and `ConfRev` mismatch.

### Slice 5 - Backend Runtime Boundary

Planned:

- Move live session ownership to backend services before any real MMS adapter exists.
- REST remains for explicit plan/read/prepare commands.
- WebSocket emits runtime state and normalized report events.
- Persist only auditable runtime actions and later test evidence; do not persist transient frontend session state as authority.

Validation:

- Backend service tests using simulator adapter only.
- No real network device tests in this slice.

### Slice 6 - Debug UI Integration

Planned:

- Show report plan, simulator state, diagnostics, and GI result in the 61850 debug view.
- Keep every hardware-affecting future action explicit.
- Keep simulator mode visually distinct from real device mode.

Validation:

- Browser verification for large SCD responsiveness.
- No implicit reserve/enable from selection alone.

### Slice 7 - MMS Adapter Spike Behind Simulator Parity

Planned only after slices 1-6 pass:

- Add an adapter boundary for IEC 61850-8-1 MMS.
- Connect/read RCB attributes against a controlled simulator or lab IED.
- Keep real enable/reserve/GI behind a backend feature flag and explicit operator action.

Validation:

- Lab-only validation checklist.
- Timeout, disconnect, reservation conflict, stale state, and cleanup evidence.

## Current Risks

- The SCD parser does not expand `DataTypeTemplates`; value typing remains shallow.
- RCB indexed instance allocation is not planned yet.
- Real MMS encoding/decoding library choice is unresolved.
- Report event persistence and test evidence linking are future backend slices.
- GOOSE and SV remain separate protocol tracks.
