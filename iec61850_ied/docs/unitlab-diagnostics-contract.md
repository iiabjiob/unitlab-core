# UnitLab diagnostics contract

Status: draft diagnostics contract for the Python/FastAPI product layer and the reusable IEC 61850 runtime.

This document defines the minimum diagnostic shape that should survive planning, session control, discovery, recovery, and evidence generation.

## Ownership

### C IEC 61850 runtime

The C runtime should emit diagnostics for:
- connect / association failure;
- discovery failure;
- partial discovery;
- subscription failure;
- report decode failure;
- stale generation rejection;
- reconnect failure;
- unexpected runtime state.

### Python/FastAPI product layer

The Python layer should:
- preserve diagnostics in API responses and evidence;
- map diagnostics into user-visible states;
- keep the original cause separate from derived verdicts;
- avoid flattening all failures into one generic "failed" state.

## Diagnostic object

Recommended fields:
- `diagnostic_id`
- `session_id`
- `endpoint_id`
- `scope`
- `category`
- `code`
- `message`
- `severity`
- `phase`
- `source_generation`
- `report_control`
- `data_set`
- `signal_id`
- `created_at`
- `details`

## Diagnostic scopes

### Runtime scope

Applies to:
- session control;
- connect / reconnect;
- discovery;
- subscription;
- report ingestion.

### Planning scope

Applies to:
- target normalization;
- subscription grouping;
- uncovered target reasons;
- fallback selection reasons.

### Evidence scope

Applies to:
- confirmation/verification timing;
- stale/late frames;
- missing feedback;
- report mismatches.

## Diagnostic categories

Suggested categories:
- `transport`
- `association`
- `discovery`
- `planning`
- `subscription`
- `report`
- `freshness`
- `evidence`
- `verdict`
- `operator`

## Stable reason codes

Diagnostics used by the product layer should prefer a stable `code` or reason code from a known vocabulary.

Suggested reason codes:
- `no_matching_endpoint`
- `no_matching_report_control`
- `no_matching_dataset`
- `discovery_failed`
- `subscription_failed`
- `report_timeout`
- `stale_generation`
- `report_out_of_order`
- `report_gap`
- `report_duplicate`
- `quality_bad`
- `evidence_out_of_window`
- `runtime_degraded`

Free text `message` and `details` are allowed, but they should supplement a stable code rather than replace it.

## Required behavior

- Every failed or partial operation should leave a structured diagnostic.
- A diagnostic should survive enough context to explain why the state changed.
- Derived states like `stale` or `partial` must preserve the original diagnostic cause.
- Old-generation report rejection should be diagnosable, not silent.
- Uncovered planning targets should have a diagnostic reason, not just a missing plan entry.
- UI-facing diagnostics should be able to map from stable codes to user messages without inspecting raw runtime internals.

## Severity

Recommended severities:
- `info`
- `warning`
- `error`
- `critical`

Suggested interpretation:
- `info`: expected operational state;
- `warning`: partial or degraded but recoverable;
- `error`: operation failed;
- `critical`: runtime cannot safely continue without recovery.

## State mapping

Diagnostics should feed, but not replace, runtime states:
- `warning` may map to `partial`, `degraded`, or `stale`;
- `error` may map to `failed`;
- `critical` may map to `failed` plus immediate recovery requirement.

## Evidence rules

- Evidence must reference the originating diagnostic when a verdict is failed, timed out, or otherwise does not satisfy the policy window.
- Diagnostics should not be overwritten by a later successful update; they should remain part of the trace.
- A successful recovery should add a new diagnostic/state transition rather than erasing the old one.

## Contract invariants

- Diagnostics must be stable enough to show in logs and API responses.
- One diagnostic should explain one root problem, not multiple unrelated issues.
- Product verdict logic must not rewrite the runtime diagnostic payload.
- Late frames, stale updates, and partial discoveries must all remain visible in diagnostics.

## Recommended Python modules

- `app/runtime/diagnostics.py`
- `app/runtime/sessions.py`
- `app/runtime/evidence.py`
- `app/planning/subscriptions.py`
- `app/planning/targets.py`
