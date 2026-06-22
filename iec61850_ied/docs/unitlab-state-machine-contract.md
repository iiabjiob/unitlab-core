# UnitLab state machine contract

Status: draft state machine contract for the Python/FastAPI product layer.

This document defines the canonical state transitions for runtime sessions and product-level verification execution.

## Scope

The state machine is split into two coupled but separate machines:

1. Runtime session state
2. Verification execution state

They should be related, but they are not the same object.

## Runtime session states

Suggested states:
- `idle`
- `connecting`
- `associated`
- `discovering`
- `discovered`
- `subscribing`
- `reporting`
- `reconnecting`
- `degraded`
- `failed`
- `closed`

### Runtime session transition rules

- `idle` -> `connecting`
- `connecting` -> `associated`
- `associated` -> `discovering`
- `discovering` -> `discovered`
- `discovered` -> `subscribing`
- `subscribing` -> `reporting`
- `reporting` -> `reconnecting`
- `reporting` -> `degraded`
- `reporting` -> `failed`
- `reconnecting` -> `connecting`
- `reconnecting` -> `discovering`
- `reconnecting` -> `subscribing`
- any active state -> `closed`
- any active state -> `failed` on unrecoverable error

## Verification execution states

Suggested state axes:

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

### Workflow transition rules

- `draft` -> `planned`
- `planned` -> `preparing`
- `preparing` -> `armed`
- `armed` -> `running`
- `running` -> `awaiting_confirmation`
- `awaiting_confirmation` -> `completing`
- `completing` -> `completed`
- any active state -> `failed` on unrecoverable error
- any active state -> `aborted`

### Evidence / verdict rules

- `observed` means the expected feedback path was seen;
- `pass` means evidence satisfied timing, freshness, quality, and policy;
- `late` means feedback was observed after the allowed window;
- `timeout` means no valid evidence arrived in time;
- `stale` means the evidence source was not live enough to trust;
- `invalid` means the evidence was malformed or not trustworthy;
- `out_of_window` means the evidence was observed but failed timing policy;
- `inconclusive` means evidence existed but was insufficient to decide;
- `fail` means the overall verdict is negative;
- `pending` is the state before enough evidence exists;
- `aborted` means the operator or system stopped the workflow intentionally.

## Coupling rules

- Runtime `reconnecting` should usually force execution evidence into `stale` or `awaiting_confirmation` again, not silent success.
- Runtime `failed` should typically propagate into workflow `failed` unless the Python layer explicitly masks it as a recoverable interruption.
- Runtime `closed` should invalidate live evidence but must not delete existing evidence records.
- Runtime `discovered` does not imply execution `planned`.
- Runtime `reporting` does not imply verdict `pass`.

## Freshness interaction

- A signal can be `live` while execution is `awaiting_confirmation`.
- A signal can be `stale` while evidence remains preserved.
- A session reconnect can move signals from `stale` back to `live`, but the execution state should still require fresh evidence.

## Failure mapping

Suggested runtime failure causes:
- `connect_failed`
- `discovery_failed`
- `subscription_failed`
- `report_decode_failed`
- `session_lost`
- `generation_rejected`
- `operator_stop`

Suggested execution failure causes:
- `planning_failed`
- `no_matching_target`
- `no_matching_report_control`
- `runtime_failed`
- `timeout`
- `evidence_missing`
- `evidence_mismatch`
- `aborted_by_operator`

## Invariants

- No state should jump directly from `draft` to `pass`.
- No stale update should automatically become `pass`.
- A terminal state should not be silently overwritten by a new active state without an explicit new run or reconnect.
- State transitions must be explicit in events and diagnostics.

## Suggested Python modules

- `app/runtime/state_machine.py`
- `app/runtime/sessions.py`
- `app/runtime/execution.py`
- `app/runtime/events.py`
