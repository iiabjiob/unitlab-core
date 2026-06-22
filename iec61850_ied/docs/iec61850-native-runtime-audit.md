# IEC 61850 Native Client/Server Protocol Audit

Status: current-state audit after the native IEC 61850 MMS client/server hardening slice.

## Scope

This note covers the current IEC 61850 native MMS client/server/protocol layer in `src/server/`, `src/model/`, and the surrounding tests in `tests/`.

It documents:
- what is already strong and production-shaped;
- where the current design still has bottlenecks or incomplete semantics;
- what is must-have versus optional next work.

It is intentionally lower-level than the session/runtime audit:
- client association, discovery, report decoding, and RCB selection policy;
- server browse, RCB exposure, and report emission shape;
- protocol-model boundaries and failure semantics.

## What is already strong

### Client side

- Session ownership is explicit.
- `SessionRuntime` owns desired state, live state, discovery snapshot, and signal runtime.
- `SessionWorker` owns the reconcile loop and prevents duplicate worker ownership for the same session.
- Reconnect is derived from live/runtime failure, not from a separate reconnect intent flag.
- Signal updates are protocol-independent once mapped from report entries.
- Signal freshness is explicit.
- Stale signals keep their last good value, quality, timestamp summary, and provenance.
- Old connection generations are rejected by the signal cache so late frames cannot overwrite the current live cache.
- Report sequence handling is no longer global-only.
- Per-RCB report sequence state exists.
- `SubSqNum` is represented in the report model and server report encoding.
- Report health is tracked and now feeds signal staleness on degraded report ordering.
- Focused tests exist for session lifecycle, worker ownership, signal freshness, generation guards, report decoder coverage, and per-RCB report sequence behavior.

### Server side

- The server exposes buffered and unbuffered report control fields explicitly enough for the current discovery/report flow.
- `RptEna`, `GI`, and report emission paths are wired through the native server runtime rather than being UI-only concepts.
- The server now emits `SqNum` and `SubSqNum` in a shape that the client can consume without special-case hacks.
- Discovery-visible `SubSqNum` and report-control metadata are represented in the browse/report model.
- The server-side report runtime and browse service are aligned with the current client expectations for the report-oriented flow.

### Protocol boundaries

- MMS report entries are mapped into signal/runtime state outside the signal cache itself.
- The signal cache is protocol-independent once the report layer has built a generic update.
- Stale generations are ignored before they can overwrite live state.
- Report health and sequence policy are treated as transport/runtime concerns, not UI concerns.

## Current bottlenecks and gaps

### Client side

- `report_health` is still a runtime health signal, not a full report-quality model.
- Degraded report health currently marks the live cache stale, but there is still no full policy for every possible report anomaly.
- Report sequence tracking is strong for the current flow, but not yet a complete generic report engine.
- Multi-RCB behavior is supported at the state level, but the selection/recovery policy is still narrow and tuned to the current runtime shape.
- Multi-session support is structurally prepared, but not yet exercised as a fleet-scale operating mode.
- Discovery is much better than before, but it is still not a full generic MMS model explorer.
- Reconnect/re-subscribe recovery exists, but it is still a minimal recovery path rather than a full backoff and recovery policy.
- The current runtime still uses synchronous execution paths in places where a future async stop/cancel model will be needed.

### Server side

- The server-side browse/discovery model is still report-oriented, not a full typed MMS model explorer.
- Browse and discovery are not yet exhaustive across all logical devices, nodes, datasets, and report-control variants in the way a tool like IEDScout presents them.
- The current server model is sufficient for the working flow, but it still has room for fuller MMS shape coverage and stricter typed-node fidelity.
- The server-side report/control surfaces are still tailored to the current test device model, not a generalized IEC 61850 data browser.

### Protocol boundaries

- `report_health` is still a runtime health signal, not a full report-quality model.
- Degraded report health currently marks the live cache stale, but there is still no full policy for every possible report anomaly.
- Discovery still has edge cases around structured and uncommon MMS typeSpecification shapes.
- The current runtime still uses synchronous execution paths in places where a future async stop/cancel model will be needed.

## Must-have next work

### Client side

- Make report-health-to-stale behavior fully explicit across session and signal layers.
- Finish multi-RCB recovery rules so reconnect does not regress selected RCB state.
- Add stronger stale-generation and late-frame rejection coverage for reconnect edges.
- Expand report anomaly handling from "good enough for the current flow" to a stable policy for duplicates, gaps, and out-of-order delivery.
- Keep discovery from aborting on one malformed branch while still recording useful diagnostics.
- Continue separating protocol mapping from runtime state so MMS-specific details do not leak into generic signal/session APIs.

### Server side

- Keep the report model aligned with the client's expected `SqNum`/`SubSqNum`/`OptFlds`/`GI` contract.
- Keep the browse model aligned with what the client can actually consume during discovery and RCB selection.
- Make the server emit enough typed metadata to survive discovery against real IED-like shapes without collapsing structures into one-off cases.

## Optional but desirable next work

### Client side

- Add richer report anomaly metrics and counters for diagnostics.
- Add a fuller discovery snapshot/versioning model for later persistence.
- Add a clearer session record object if the parallel runtime arrays become harder to maintain.
- Add an explicit async/cancellation-friendly worker stop model.
- Add more coverage for unusual MMS typeSpecification shapes beyond the current accepted set.
- Add a broader multi-session test matrix once the one-session recovery path is fully stable.

### Server side

- Add more server-side typed-node coverage for uncommon structures, arrays, and time-like leaves.
- Add stronger server-side browse coverage for more logical-device and report-control variants.
- Add more explicit server diagnostics for unsupported typeSpecification branches.

### Protocol boundaries

- Add richer report anomaly metrics and counters for diagnostics.
- Add a fuller discovery snapshot/versioning model for later persistence.
- Add a clearer session record object if the parallel runtime arrays become harder to maintain.
- Add an explicit async/cancellation-friendly worker stop model.
- Add more coverage for unusual MMS typeSpecification shapes beyond the current accepted set.
- Add a broader multi-session test matrix once the one-session recovery path is fully stable.

## Validation expectations

- Keep using focused runtime tests for session, worker, signal, and report decoder behavior.
- Add one or two targeted tests when a gap is closed rather than waiting for a full-suite rewrite.
- For any reconnect or report-policy change, verify both current live signal state and stale preservation of last known good state.
- For any client/server protocol change, verify both the server-emitted shape and the client-decoded runtime effect.

## Runtime risks to watch

- If a future refactor reintroduces reconnect intent as a business flag, recovery will become harder to reason about.
- If report sequence policy grows without a per-RCB boundary, multi-RCB behavior will regress.
- If signal freshness and report health drift apart again, the live cache will become misleading after disconnect/reconnect.
- If discovery starts swallowing malformed branches silently, the model will look complete while hiding real device gaps.
- If the server-side browse/report model drifts from the client-side decoder assumptions, the working flow will become brittle again.
- If typed discovery remains only "good enough" for the happy path, real IED models will keep exposing edge-case failures.

## File references

- `src/server/native_wire_client.c`
- `src/server/native_wire_client_session.c`
- `src/server/native_wire_session_runtime.c`
- `src/server/native_wire_session_worker.c`
- `src/server/native_wire_signal_runtime.c`
- `src/model/model_plan.c`
- `src/server/unitlab_mms_server_runtime.c`
- `src/server/unitlab_mms_server_report_service.c`
- `src/server/services/unitlab_mms_server_browse_service.c`
- `tests/native_wire_session_runtime_test.c`
- `tests/native_wire_signal_runtime_test.c`
- `tests/native_wire_client_report_decoder_test.c`
- `tests/native_wire_client_session_test.c`
