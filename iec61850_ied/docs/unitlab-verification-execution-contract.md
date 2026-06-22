# UnitLab verification execution contract

Status: draft execution contract for the Python/FastAPI product layer.

This document defines the end-to-end control flow that turns selected signal-list rows into confirmed or verified test evidence.

## Ownership

### Python/FastAPI product layer

Owns:
- signal selection;
- target normalization;
- subscription planning;
- execution orchestration;
- verdict computation;
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
8. Python computes a test verdict from evidence and timing policy.

## Execution states

The product layer should expose explicit states rather than a single boolean:

- `draft`
- `planned`
- `connecting`
- `discovering`
- `subscribing`
- `armed`
- `running`
- `awaiting_confirmation`
- `confirmed`
- `verified`
- `stale`
- `timed_out`
- `unconfirmed`
- `failed`
- `aborted`
- `completed`

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

### `confirmed`

The expected IEC 61850 feedback was observed.

### `verified`

The feedback was observed inside the allowed timing window and is considered successful.

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
- reason.

Evidence must survive reconnect and stale transitions even when the live signal cache changes later.

### `stale`

The last known evidence exists, but the source session/report generation is no longer live.

### `timed_out`

No valid feedback arrived inside the allowed window.

### `unconfirmed`

A feedback path was expected, but evidence was insufficient or incomplete.

### `failed`

The workflow failed due to runtime, protocol, planning, or evidence error.

### `aborted`

The operator or system stopped the workflow intentionally.

### `completed`

The workflow finished and the final verdict is stable.

## Contract invariants

- A target must remain traceable back to the original signal-list row.
- A plan must remain traceable back to the target(s) that produced it.
- A report update must remain traceable back to session, endpoint, report-control, and data-set identity.
- Evidence must not be destroyed when a later state arrives.
- Late or stale report updates must not rewrite a newer session generation.
- Evidence must be reconstructable from runtime report updates and session provenance.
- Verdicts must come from evidence and policy, not from UI convenience state.

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
