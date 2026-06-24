# UnitLab verification execution contract

Status: draft execution contract for the Python/FastAPI product layer.

This document defines the end-to-end control flow that turns selected signal-list rows into observed evidence and derived verdict state.

## Ownership

### Python/FastAPI product layer

Owns:
- signal selection;
- target normalization;
- subscription planning;
- execution orchestration;
- verdict state computation;
- evidence persistence;
- user-facing status.

Execution ownership:
- use a backend-owned worker/task inside the Python/FastAPI process for long-lived verification orchestration;
- keep the process boundary out of the product contract for now;
- introduce a separate process only if a measured scale or isolation need appears later.

### C IEC 61850 runtime

Provides:
- connect / discover / subscribe / reconnect primitives;
- report decoding;
- live signal freshness;
- stale-generation protection;
- diagnostics and runtime snapshots.

The C runtime does not own test verdict policy.

## End-to-end execution flow

1. User selects signal-list rows.
2. Python normalizes rows into `VerificationTarget` records.
3. Python groups targets into a `SubscriptionPlan`.
4. Python asks the C runtime to prepare the relevant IEC 61850 session(s).
5. Python executes the selected test action.
6. C runtime receives reports and updates live signal state.
7. Python converts report updates into durable `SignalVerificationEvidence` records.
8. Python computes verdict state from evidence and timing policy.
9. Python computes verification confidence from provenance, coverage, runtime health, and source quality.

For multi-IED runs, the source of truth for runtime health is the set of `session_snapshots`; any top-level runtime state should be treated only as a derived aggregate summary.

The workflow should preserve the source `ExecutionContext` so the run remains reconstructable later.

## Evidence immutability

Evidence records should be append-only once created.

Rules:
- later report updates may create new evidence records or update aggregate summaries;
- later report updates should not destructively overwrite the original evidence trail;
- each evidence record must remain linked to source session, generation, report-control, and dataset identity;
- a verdict may be recomputed from evidence + policy, but the original evidence remains intact.

## PR4 - First auto evidence-driven flow

The first auto flow should be the smallest end-to-end product loop:
- a planned target set is armed;
- the output trigger fires;
- the runtime waits for a report-based confirmation;
- evidence is collected;
- the verdict is derived from evidence plus timing policy.

Required behavior:
- the active session and plan must be visible before the trigger fires;
- the runtime must keep waiting for confirmation until the allowed window expires or evidence arrives;
- evidence status and verdict are distinct;
- `observed` means the expected feedback path was seen;
- `pass` means evidence satisfied timing, freshness, quality, and policy;
- `late` means feedback was observed after the allowed window;
- `timeout` means no valid confirmation arrived in time;
- `stale` means the evidence source was not live enough to trust;
- `invalid` means the evidence was malformed or not trustworthy;
- `out_of_window` means the evidence was observed but failed timing policy;
- `inconclusive` means evidence existed but was insufficient to decide;
- `fail` means the overall verdict is negative;
- `pending` is the state before enough evidence exists;
- `aborted` means the operator or system stopped the workflow intentionally.

`verdict_state` answers whether the verification passed.
`verification_confidence` answers how strong the proof is behind that verdict.
These are intentionally separate fields and should not be collapsed into one status.

## Confidence taxonomy

Use a normalized `verification_confidence` value to describe proof strength:

- `exact_iec61850`: exact live report-control and dataset confirmation from a real IED.
- `exact_report_match`: exact report-control or dataset match, but the source is not yet proven to be a real IED.
- `discovery_match`: matched via discovery metadata or discovery-derived binding.
- `simulated_fallback`: simulator or fallback-planned runtime produced the matching report.
- `simulated`: simulator-generated report without stronger binding.
- `degraded`: the verdict is still explainable, but recovery, stale state, or partial coverage reduced trust.
- `unknown`: confidence has not been classified yet.

Use a normalized `confidence_reason` code rather than ad-hoc prose:

- `exact_report_control_match`
- `exact_dataset_match`
- `discovery_match`
- `fallback_planning_used`
- `simulator_generated_report`
- `degraded_recovery_state`
- `partial_coverage`
- `unknown`

The confidence reason should be machine-stable and should not replace diagnostics or human-readable summaries.

### Aggregation policy

For future multi-signal and multi-IED runs:

- step confidence is derived from the strongest applicable evidence/source classification for that step;
- run confidence is the weakest confidence among the contributing steps after applying runtime-health modifiers;
- if any contributing step is `degraded`, the run confidence cannot exceed `degraded`;
- if any contributing step is `simulated_fallback`, the run confidence cannot exceed `simulated_fallback`;
- if all contributing steps are `exact_iec61850`, the run confidence is `exact_iec61850`;
- a passing verdict does not raise confidence on its own.

This aggregation rule is deterministic and should be stable for identical inputs.

The product layer should preserve the exact evidence trail used for the verdict.

## PR5 - Multi-IED selected-group flow

One selected group may span multiple IEDs and therefore multiple sessions or report controls.

Required behavior:
- split the group into per-IED execution tracks;
- keep each session isolated;
- preserve evidence per IED and per report-control path;
- aggregate the user-visible result without collapsing per-IED detail;
- keep the single-IED case unchanged.

The final verdict for the group should be explainable from per-IED evidence, not from a merged boolean.

## PR6 - Recovery hardening

Recovery hardening should make reconnect and stale handling boring and explicit:
- preserve desired work while recovery is in flight;
- keep prior evidence and stale signals visible;
- reject late frames from old generations;
- keep reconnect idempotent for one session at a time;
- avoid corrupting current evidence when recovery overlaps active confirmation windows.

Recovery should remain a product-layer decision driven by runtime diagnostics, freshness, and session state.

## State taxonomy

Do not collapse the following into one flat state field:

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

## State meaning

### `draft`

Targets have been selected, but no plan exists yet.

### `planned`

A deterministic subscription plan exists, but no runtime session action has started yet.

### `connecting`

The runtime is establishing association or reconnecting a session.

### `discovering`

The runtime is collecting discovery data needed for execution or validation.

### `subscribing`

The runtime is reserving and enabling the selected report control(s).

### `armed`

The runtime is ready to observe the test trigger but has not started the active step yet.

### `running`

The simulated output or test action is active.

### `awaiting_confirmation`

The test action has fired and the system is waiting for report-based confirmation.

### `signal evidence`

Evidence is a durable record, not a transient UI flag.

Each evidence record should preserve:
- target identity;
- canonical signal path;
- actual report path;
- source session and generation;
- source report control and data set;
- received timestamp;
- timing window result;
- freshness / stale context;
- reason;
- evidence status.

Evidence must survive reconnect and stale transitions even when the live signal cache changes later.

## Step evidence accumulation

A verification step may accumulate multiple evidence records over time, including:
- the first observed report;
- a late report;
- a stale or duplicate report;
- a recovered report after reconnect.

Step state should retain the evidence trail, not collapse it into one path-only field.

### `completed`

The workflow finished and the final verdict is stable.

## Verification / recovery invariants

- A verification run should not lose the evidence trail when one session reconnects.
- A multi-IED group should not hide one IED failure behind a successful peer.
- Recovery should preserve prior targets and evidence while restoring live confirmation ability.
- Late or old-generation report updates must remain rejected and diagnosable.

## Contract invariants

- A target must remain traceable back to the original signal-list row.
- A plan must remain traceable back to the target(s) that produced it.
- A report update must remain traceable back to session, endpoint, report-control, and data-set identity.
- Evidence must not be destroyed when a later state arrives.
- Late or stale report updates must not rewrite a newer session generation.
- Evidence must be reconstructable from runtime report updates and session provenance.
- Verdicts must come from evidence and policy, not from UI convenience state.
- `evidence_status` and `verdict_state` must remain separate and independently inspectable.
- `verification_confidence` and `confidence_reason` should be carried alongside verdict state and remain separate from evidence status.
- `ExecutionContext` must be carried with the run for later reconstruction.

## Summary flow

SignalListRow -> VerificationTarget -> SubscriptionPlan -> Runtime Session / Report Updates -> SignalVerificationEvidence -> Verdict -> UI/API

The runtime/explanation layer may additionally derive `verification_confidence` from the same trail, but that confidence must remain separate from verdict state.

Ownership:
- SignalListRow: UI/persistence
- VerificationTarget: Python/FastAPI product layer
- SubscriptionPlan: Python/FastAPI product layer
- Runtime Session / Report Updates: reusable IEC 61850 C runtime
- SignalVerificationEvidence: Python/FastAPI product layer
- Verdict: Python/FastAPI product layer
- UI/API projection: Python/FastAPI product layer

## Runtime interaction points

The Python layer should rely on the C runtime for:
- session status;
- discovery availability;
- selected report control state;
- last report summary;
- signal freshness;
- diagnostics.

The Python layer should not derive verdicts from:
- raw UI selection state;
- transient frontend focus state;
- internal C-only session mutation details.

## Failure handling

The product layer should treat these as explicit failure reasons:
- no matching report control;
- no matching data set;
- connection failure;
- discovery failure;
- subscription failure;
- late or stale report generation;
- timeout;
- missing report confirmation;
- malformed report evidence.

Failure states should remain inspectable in the product API.

## Recommended Python modules

- `app/runtime/execution.py`
- `app/runtime/evidence.py`
- `app/runtime/sessions.py`
- `app/runtime/verdicts.py`
- `app/planning/targets.py`
- `app/planning/subscriptions.py`
