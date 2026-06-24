# UnitLab operating model

Status: draft product operating model for the Python/FastAPI layer above the reusable IEC 61850 C runtime.

This document describes how the product should behave end-to-end when a user selects signals, requests verification, and waits for evidence-driven verdict state.

## Core workflow

1. The user selects one or more signal-list rows.
2. The product normalizes those rows into verification targets.
3. The planner groups targets into per-IED / per-RCB subscription plans.
4. Before execution, the product checks network readiness and endpoint reachability and shows a clear operator hint when the host is not on a usable subnet.
5. The product may auto-suggest or auto-configure a suitable network adapter/IP configuration from signal-list and allocation data, while still allowing manual override.
6. The operator presses `Run Test` once after selection and allocation.
7. The runtime opens sessions, discovers where needed, creates subscriptions, executes the chosen scenario internally, and waits for reports.
8. The report stream is converted into signal/evidence state.
9. The verdict engine derives `pending`, `pass`, `fail`, `inconclusive`, or `aborted` from evidence, freshness, and policy.
10. The confidence model derives proof strength independently from the verdict.

## Ownership boundaries

### UI

- collects user intent;
- shows current state and results;
- surfaces network readiness and next-step guidance before execution;
- does not own execution truth;
- does not decide planner or verdict semantics.

### Python/FastAPI product layer

- owns signal normalization;
- owns subscription planning;
- owns evidence and verdict-state models;
- owns test-run state;
- owns recovery policy above the reusable runtime;
- owns persistence and API orchestration.

### Reusable IEC 61850 C runtime

- owns association/connect/disconnect/reconnect primitives;
- owns discovery snapshots;
- owns session snapshots;
- owns subscription snapshots;
- owns RCB selection and report enablement execution;
- owns report ingestion and signal freshness;
- owns protocol diagnostics and stable source identity.

### Execution ownership decision

- The product layer should use a backend-owned worker inside the Python/FastAPI process for long-lived verification orchestration.
- The main operator path should stay as a single `Run Test` action; scenario choice belongs to the product layer, not the operator.
- A separate process is not required for the current slice and would add IPC, serialization, and duplicate ownership complexity without solving a known bottleneck yet.
- Keep the process boundary available as a future deployment option if isolation or scale eventually requires it.

## Product states

The product should keep these state axes explicit:

### Workflow state

- `draft`
- `planned`
- `preparing`
- `armed`
- `running`
- `awaiting_confirmation`
- `completing`
- `completed`
- `aborted`
- `failed`

### Runtime/session state

- `connecting`
- `discovering`
- `subscribing`
- `reporting`
- `reconnecting`
- `degraded`
- `closed`

### Subscription state

- `pending`
- `reserving`
- `enabled`
- `reporting`
- `reconnecting`
- `degraded`
- `closed`

### Evidence status

- `none`
- `observed`
- `stale`
- `timeout`
- `invalid`
- `late`
- `out_of_window`

### Verdict state

- `pending`
- `pass`
- `fail`
- `inconclusive`
- `aborted`

### Verification confidence

- `exact_iec61850`
- `exact_report_match`
- `discovery_match`
- `simulated_fallback`
- `simulated`
- `degraded`
- `unknown`

The runtime may use lower-level session and signal states internally, but the product layer should expose the higher-level state to the UI and API consumers.

## Evidence rules

- evidence must be tied to a source report and source session/generation;
- evidence must preserve received time and timing window data;
- evidence must survive reconnect and stale transitions;
- evidence must not be overwritten destructively by later updates;
- verdicts must be explainable from stored evidence fields.
- evidence status and verdict state must remain separate.
- verification confidence must be explainable from provenance, plan coverage, runtime health, and source quality.
- verification confidence must remain separate from verdict state.
- verification steps should reference the session that carried the report and the subscription that owned the report-control stream.

## Recovery rules

- reconnect is a recovery mechanism, not a business intent;
- the product should preserve desired verification work across reconnect;
- late frames from old generations must not corrupt current evidence;
- stale signals and degraded sessions must remain visible to the product layer.

## Contract with the reusable runtime

The product layer should pass the runtime:
- endpoint identity;
- desired session action;
- selected report-control / dataset references;
- timeout / timing window policy;
- explicit source identity when available;
- local network readiness and adapter guidance when the run depends on a real MMS target.

The runtime should return:
- session snapshot;
- subscription snapshots;
- discovery snapshot;
- report updates;
- freshness state;
- diagnostics.

## Why session and subscription must stay separate

If one physical session is duplicated once per subscription, the product will eventually miscount live connections, overstate recovery scope, and blur whether a failure belongs to transport or to a single report-control stream.

That becomes dangerous when:
- one IED has one MMS association;
- the association carries multiple report controls;
- one report-control recovers while another remains stale;
- a multi-signal run needs clean per-subscription evidence.

The contract should therefore treat session count and subscription count as different dimensions, not as interchangeable duplicates.

## Out of scope

- fleet scaling;
- historian;
- alarms;
- UI implementation details;
- protocol-specific verdict logic;
- SCD generation;
- generic workflow engine abstractions.

## Confidence policy

- `verdict_state` answers whether the selected target or run passed.
- `verification_confidence` answers how strong the proof is behind that verdict.
- A pass with simulator fallback is valid but not equivalent to a pass confirmed by an exact live IEC 61850 report-control path.
- Confidence must not be inferred from free-text summary strings.
- Confidence should be derived from the weakest contributing step after considering runtime health and coverage.

## Related docs

- `docs/iec61850-unitlab-roadmap.md`
- `docs/iec61850-unitlab-readiness-matrix.md`
- `docs/unitlab-product-slice-tracker.md`
- `docs/unitlab-runtime-integration-contract.md`
- `docs/unitlab-signal-planning-contract.md`
- `docs/unitlab-verification-execution-contract.md`
- `docs/unitlab-recovery-contract.md`
- `docs/unitlab-api-surface.md`
- `docs/unitlab-api-payload-shapes.md`
