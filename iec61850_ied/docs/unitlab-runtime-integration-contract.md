# UnitLab runtime integration contract

Status: draft boundary contract between the Python/FastAPI product layer and the reusable IEC 61850 C runtime.

This document defines the runtime-facing data exchange that the Python service should use when orchestrating IEC 61850 verification workflows.

## Boundary summary

### C runtime responsibilities

The C runtime provides:
- IEC 61850 discovery snapshots;
- report-control metadata;
- session/connect/disconnect/reconnect primitives;
- report ingestion and live signal freshness;
- stable protocol references and diagnostics.

### Python/FastAPI responsibilities

The Python application provides:
- signal-list selection;
- verification target normalization;
- subscription planning;
- evidence generation;
- verdict computation;
- persistence and API orchestration.

## Runtime objects

### Discovery snapshot

The C runtime should expose a snapshot with:
- endpoint identity;
- discovery version / snapshot id;
- discovery `created_at_ms` or equivalent freshness marker;
- discovery source hash or equivalent stable fingerprint when available;
- logical devices;
- logical nodes;
- data sets;
- report controls;
- discovered signals or signal references where available;
- summary counts;
- diagnostic state if discovery was partial.

The Python layer may treat discovery as:
- `available`
- `partial`
- `failed`

but must preserve the underlying snapshot and reasons.

### Verification target input

The Python layer should build a verification target object before planning.

Required fields:
- `signal_id` or row identity;
- `signal_reference`;
- `signal_path`;
- `endpoint_id`;
- `ied_name`;
- `access_point_name`;
- `logical_device_inst`;
- `logical_node_name`;
- `expected_feedback_path`;
- `timeout_ms`;
- `window_ms`;
- optional source metadata from SCD or discovery.

The C runtime does not own this object, but it should be able to consume the endpoint / IED identity and report-control references that come out of the plan.

### Subscription plan input

The Python layer should build a plan with:
- one or more groups per endpoint / report control;
- explicit report-control reference;
- explicit data-set reference;
- group reason;
- uncovered target list with reasons.

The C runtime should only receive the execution-side subset of the plan:
- which endpoint to connect to;
- which report control to reserve/enable;
- which data-set/report-control references to use;
- whether GI is requested;
- any timeouts required for execution.

The C runtime must not infer product-level verdict semantics.

### Report update / evidence input

The C runtime should emit report updates with:
- source session / generation;
- endpoint identity;
- report control identity;
- data set identity;
- report reason;
- data reference / signal path;
- value / quality / timestamp summary;
- received time;
- freshness state;
- diagnostic data if the frame was stale or malformed.

The Python layer can then create evidence records and compute verdicts.

## Expected API shape

The exact transport can be REST, queue, RPC, or direct service call, but the logical contract should remain stable.

Suggested service boundaries:

### Discovery service

Input:
- endpoint identity.

Output:
- discovery snapshot;
- diagnostics;
- summary counts.

### Planning service

Input:
- signal-list selection;
- discovery snapshot when available;
- optional SCD-derived hints.

Output:
- verification targets;
- subscription plan;
- uncovered items with reasons.

### Session control service

Input:
- endpoint identity;
- desired session action (`connect`, `discover`, `subscribe`, `reconnect`, `disconnect`).

Output:
- live session state;
- selected RCB state;
- diagnostics;
- current freshness summary.

The session snapshot may also expose a compact signal-cache summary:
- total cached signals;
- live signal count;
- stale signal count;
- unknown-freshness signal count;
- signal update count;
- signal change count;
- stale-generation drop count.

If the runtime has an explicit subscription intent, the snapshot should also expose the selected RCB key used for recovery/re-subscription.

If available, the session snapshot should also expose a compact report-health summary:
- `unknown` while no valid report has been observed;
- `live` when the report stream is healthy;
- `degraded` when report health has been marked stale or unhealthy;
- report-health reason text when degraded.

If available, the session snapshot should also expose in-flight operation flags:
- connect;
- discover;
- subscribe;
- reconnect.

### Report stream / state update service

Input:
- C runtime report update events.

Output:
- persisted signal state;
- evidence state;
- verdict-relevant status.

## Contract invariants

- Planning must be deterministic for the same input.
- Discovery snapshots must not be mutated in place into a different identity.
- One report-control execution should not silently affect another group.
- Late or stale report updates must not overwrite the current live generation.
- Report health and signal freshness must remain visible to Python.
- Product verdict logic must not live inside the C runtime.

## Current repository status

This repository currently implements the reusable C runtime and its docs, but not the Python/FastAPI service layer.

The Python application should implement this contract before product-level verification work resumes.
