# UnitLab product slice tracker

Status: living execution tracker for the UnitLab workflow above the reusable IEC 61850 C runtime.

This tracker is intentionally product-layer only:
- Python/FastAPI owns planning, evidence, and verdicts;
- the C runtime owns reusable IEC 61850 discovery/session/report primitives;
- do not move product-specific heuristics into the C layer.

Execution ownership:
- use a backend-owned worker inside the Python/FastAPI process for long-lived verification orchestration;
- do not introduce a separate process for the current slice;
- revisit process isolation only if scale, crash containment, or socket ownership becomes a measured problem.

## Slice order

The workflow should be built in this order:
1. signal selection normalization;
2. subscription planning;
3. evidence capture;
4. first auto verification flow;
5. multi-IED selected-group flow;
6. recovery hardening.

## Slice tracker

### PR1 - Signal list to verification targets

Status: completed.

Input:
- selected signal-list rows;
- optional SCD hints;
- endpoint metadata when present.

Output:
- normalized verification targets;
- stable source row identity;
- expected feedback path;
- timeout/window data.

Done when:
- a selected row becomes a deterministic target object;
- the target can be logged and inspected without re-reading the UI state;
- SCD remains optional.
- current backend preview path emits normalized verification targets from the signal test-run job preview.

### PR2 - Subscription planner

Status: not started.

Input:
- verification targets;
- discovery snapshot when available;
- optional SCD hints.

Output:
- per-IED / per-RCB subscription plan;
- uncovered items with reasons;
- explicit source classification.

Done when:
- the same input produces the same plan;
- multi-IED groups are split cleanly;
- uncovered targets are not hidden.
- each grouped target keeps a stable source classification.

### PR3 - Verification evidence model

Status: not started.

Input:
- runtime report updates;
- source identity and freshness data;
- source generation and report provenance.

Output:
- evidence records tied to source report/path;
- explicit provenance;
- latency and freshness fields;
- durable evidence set per test run;
- verdict-ready state.

Done when:
- evidence survives reconnect and stale transitions;
- the UI is not the source of truth.
- the same runtime evidence can be reconstructed from persisted product data.

### PR4 - First auto verification flow

Status: not started.

Input:
- selected target set;
- active runtime session;
- test trigger from UnitLab.

Output:
- observed / pass / timeout / fail result;
- evidence-backed decision;
- visible timing window result.

Done when:
- a single signal can complete the full feedback loop;
- the verdict is explained by stored evidence.
- the run remains inspectable as a single `VerificationRun`.
- the step state remains traceable back to the originating signal row.

### PR5 - Multi-IED selected-group flow

Status: not started.

Input:
- one selected group that spans multiple IEDs.

Output:
- isolated per-IED execution;
- coherent user-visible result for the whole group.

Done when:
- the planner and runtime keep per-IED failure detail;
- single-IED behavior does not regress.
- each IED/session keeps its own evidence trail and verdict explanation.

### PR6 - Recovery hardening

Status: not started.

Input:
- reconnect events;
- stale signals;
- report gaps;
- partial failures.

Output:
- stable recovery behavior;
- preserved evidence;
- rejected late/old frames.

Done when:
- reconnect does not corrupt current evidence;
- stale and gap conditions remain visible.
- old-generation frames are rejected and diagnosable.
- one IED reconnect does not invalidate other IED sessions in the same selected group.

## Mandatory vs optional

### Mandatory

- signal-list to IED/report planning without manual tuning;
- report evidence in the verdict path;
- reconnect preserving desired work;
- stale/late frames not corrupting current evidence;
- explicit explanation for observed / pass / stale / timeout / fail.

### Optional

- richer anomaly metrics;
- broader discovery snapshot versioning;
- more server-side typed discovery breadth;
- more multi-session diagnostics;
- broader golden-capture automation.

## Related docs

- `docs/iec61850-unitlab-roadmap.md`
- `docs/iec61850-unitlab-readiness-matrix.md`
- `docs/unitlab-runtime-integration-contract.md`
- `docs/unitlab-signal-planning-contract.md`
- `docs/unitlab-api-payload-shapes.md`
- `docs/unitlab-verification-execution-contract.md`
- `docs/unitlab-recovery-contract.md`
- `docs/unitlab-diagnostics-contract.md`
