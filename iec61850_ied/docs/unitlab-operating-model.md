# UnitLab operating model

Status: draft product operating model for the Python/FastAPI layer above the reusable IEC 61850 C runtime.

This document describes how the product should behave end-to-end when a user selects signals, requests verification, and waits for evidence-driven verdict state.

## Core workflow

1. The user selects one or more signal-list rows.
2. The product normalizes those rows into verification targets.
3. The planner groups targets into per-IED / per-RCB subscription plans.
4. The runtime opens sessions, discovers where needed, subscribes, and waits for reports.
5. The report stream is converted into signal/evidence state.
6. The verdict engine derives `pending`, `pass`, `fail`, `inconclusive`, or `aborted` from evidence, freshness, and policy.

## Ownership boundaries

### UI

- collects user intent;
- shows current state and results;
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
- owns RCB selection and report enablement execution;
- owns report ingestion and signal freshness;
- owns protocol diagnostics and stable source identity.

### Execution ownership decision

- The product layer should use a backend-owned worker inside the Python/FastAPI process for long-lived verification orchestration.
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

The runtime may use lower-level session and signal states internally, but the product layer should expose the higher-level state to the UI and API consumers.

## Evidence rules

- evidence must be tied to a source report and source session/generation;
- evidence must preserve received time and timing window data;
- evidence must survive reconnect and stale transitions;
- evidence must not be overwritten destructively by later updates;
- verdicts must be explainable from stored evidence fields.
- evidence status and verdict state must remain separate.

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
- explicit source identity when available.

The runtime should return:
- session snapshot;
- discovery snapshot;
- report updates;
- freshness state;
- diagnostics.

## Out of scope

- fleet scaling;
- historian;
- alarms;
- UI implementation details;
- protocol-specific verdict logic;
- SCD generation;
- generic workflow engine abstractions.

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
