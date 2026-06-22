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

Suggested states:
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

### Verification execution transition rules

- `draft` -> `planned`
- `planned` -> `connecting`
- `connecting` -> `discovering`
- `discovering` -> `subscribing`
- `subscribing` -> `armed`
- `armed` -> `running`
- `running` -> `awaiting_confirmation`
- `awaiting_confirmation` -> `confirmed`
- `confirmed` -> `verified`
- `confirmed` -> `stale`
- `awaiting_confirmation` -> `stale`
- `awaiting_confirmation` -> `timed_out`
- `awaiting_confirmation` -> `unconfirmed`
- any non-final active state -> `failed`
- any active state -> `aborted`
- any terminal state -> `completed`

## Coupling rules

- Runtime `reconnecting` should usually force verification execution into `stale` or `awaiting_recovery`, not silent success.
- Runtime `failed` should typically propagate into execution `failed` unless the Python layer explicitly masks it as a recoverable interruption.
- Runtime `closed` should invalidate live evidence but must not delete existing evidence records.
- Runtime `discovered` does not imply execution `planned`.
- Runtime `reporting` does not imply execution `verified`.

## Freshness interaction

- A signal can be `live` while execution is `awaiting_confirmation`.
- A signal can be `stale` while evidence remains preserved.
- A session reconnect can move signals from `stale` back to `live`, but the execution state should still require confirmation.

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
- `confirmation_missing`
- `evidence_mismatch`
- `aborted_by_operator`

## Invariants

- No state should jump directly from `draft` to `verified`.
- No stale update should automatically become verified.
- A terminal state should not be silently overwritten by a new active state without an explicit new run or reconnect.
- State transitions must be explicit in events and diagnostics.

## Suggested Python modules

- `app/runtime/state_machine.py`
- `app/runtime/sessions.py`
- `app/runtime/execution.py`
- `app/runtime/events.py`

