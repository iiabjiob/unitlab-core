# IEC 61850 UnitLab readiness matrix

Status: living audit of what is strong today, what still blocks productization, and what is optional for later.

This note is intentionally cross-layer:
- reusable IEC 61850 C runtime;
- UnitLab product layer in Python/FastAPI;
- end-to-end verification workflow;
- lower-level client/server protocol readiness.

It is not a replacement for the detailed protocol audit in `docs/iec61850-native-runtime-audit.md`.

## Boundary rule

Keep the C layer reusable:
- discovery snapshots;
- session/connect/disconnect/reconnect primitives;
- report ingestion;
- signal freshness;
- stable protocol diagnostics.

Keep UnitLab-specific planning and verdict logic in the product layer:
- signal-list selection;
- verification target normalization;
- subscription planning;
- evidence generation;
- verdict computation;
- persistence/API orchestration.

## Current readiness snapshot

### Native MMS client

Strong:
- association/connect/disconnect works;
- discovery reaches real IEDs and preserves useful model data;
- buffered report enablement and GI flow work for the current target shapes;
- reconnect and stale-generation protection are explicit;
- report health, freshness, and per-RCB sequence handling are integrated.

Remaining bottlenecks:
- discovery still needs broader typed-node coverage for uncommon MMS shapes;
- report anomaly policy still has room for fuller gap/out-of-order handling;
- reconnect recovery still needs product-level reconciliation around desired state.

### Native MMS server

Strong:
- exposes the report-oriented browse/report shape needed by the current client;
- supports buffered/unbuffered RCBs, `RptEna`, GI, and report emission;
- current client/server smoke path is usable for real workflow pressure tests.

Remaining bottlenecks:
- typed browse coverage is still narrower than a full exploratory MMS browser;
- some model breadth is still optimized for the working flow rather than every possible IED shape.

### Discovery engine

Strong:
- discovery is no longer a toy path;
- report-oriented discovery is good enough to drive current subscription selection;
- discovery can survive several real-world structure/time/quality shapes.

Remaining bottlenecks:
- not yet a full generic MMS model explorer;
- needs a clearer policy for rare/unsupported typeSpecification branches;
- must continue to skip malformed branches without aborting the whole model.

### Report runtime

Strong:
- report entries map into a protocol-independent signal cache;
- live/stale state is explicit;
- late frames from old generations are rejected;
- report health influences cache freshness.

Remaining bottlenecks:
- not yet a full report anomaly engine;
- report sequence and freshness policy still need product-level consumption and verdict rules.

### Session runtime

Strong:
- ownership is explicit;
- desired vs actual state is separated;
- worker ownership and reconcile loop exist;
- reconnect is derived from runtime failure rather than a business intent flag.

Remaining bottlenecks:
- reconnect/re-subscribe recovery still needs product-level reconciliation around desired work;
- async cancellation/stop refinement is still a future concern;
- one-session behavior is strong, but multi-session operating mode still needs product pressure testing.

### Signal runtime

Strong:
- signal identity is protocol-independent;
- freshness and staleness are explicit;
- source provenance and connection generation are preserved;
- old generation updates do not corrupt the current live cache.

Remaining bottlenecks:
- signal freshness still needs the rest of the product loop to consume it consistently;
- signal-to-verdict mapping belongs above the reusable runtime.

### UnitLab product layer

Partially implemented in the backend layer in this repository:
- signal normalization preview for selected signal-list rows;
- verification target materialization for test-run preview;
- grouped subscription-plan preview with uncovered-target diagnostics.

Still pending:
- verdict engine for hardware-backed runs;
- API/event orchestration for the full verification flow;
- UI integration.

Implemented in the backend layer here:
- verification evidence model.
- first auto verification flow via simulator-backed runtime path.
- multi-IED selected-group execution via simulator-backed runtime path.

This is a deliberate boundary, not a gap in the C runtime.

## Must-have next work

### For the reusable IEC 61850 layer

- keep typed discovery tolerant for rare structures and arrays;
- preserve report health and freshness semantics across reconnect;
- keep per-RCB sequence handling stable under reconnect and multiple subscriptions;
- keep late/old generations out of the live cache;
- maintain production-shaped server browse/report coverage for the current flow.

### For the UnitLab product layer

- turn selected signal-list rows into normalized verification targets;
- build a deterministic subscription planner per IED/report control;
- persist verification evidence with clear provenance;
- compute pass/fail/inconclusive/aborted verdict states from evidence, not from UI state;
- support multi-IED groups without collapsing them into one shared runtime;
- make reconnect recovery preserve desired work and evidence.

## Optional next work

### Reusable IEC 61850 layer

- richer typed-node breadth for uncommon MMS shapes;
- additional discovery snapshot/versioning metadata;
- broader golden-capture automation;
- async stop/cancellation refinements.

### UnitLab product layer

- richer diagnostics and operator explanations;
- longer-horizon fleet scaling;
- more explicit replay/report reconstruction tooling;
- advanced recovery/backoff policy.

## Slice tracking view

Recommended order:
1. verification target normalization;
2. subscription planner;
3. verification evidence model;
4. first auto test flow;
5. multi-IED selected-group flow;
6. hardening for reconnect, stale signals, and report gaps.

That sequence keeps the reusable IEC 61850 layer isolated while letting the Python/FastAPI product layer own the UnitLab-specific workflow.

## Related docs

- `docs/iec61850-unitlab-roadmap.md`
- `docs/iec61850-native-runtime-audit.md`
- `docs/mms-client-roadmap.md`
- `docs/unitlab-runtime-integration-contract.md`
- `docs/unitlab-signal-planning-contract.md`
- `docs/unitlab-api-payload-shapes.md`
- `docs/unitlab-verification-execution-contract.md`
- `docs/unitlab-recovery-contract.md`
- `docs/unitlab-diagnostics-contract.md`
- `docs/unitlab-api-surface.md`
- `docs/unitlab-event-stream-contract.md`
- `docs/unitlab-state-machine-contract.md`
- `docs/unitlab-json-schema-contract.md`
