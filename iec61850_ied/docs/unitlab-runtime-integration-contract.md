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
- stable protocol references and diagnostics;
- source generation protection;
- report-health summaries;
- low-level capability summaries for the discovered IEC 61850 model.

### Python/FastAPI responsibilities

The Python application provides:
- signal-list selection;
- verification target normalization;
- subscription planning;
- evidence generation;
- verdict computation;
- persistence and API orchestration;
- workflow state;
- evidence status;
- verdict state;
- recovery policy;
- operator-facing API status.

The Python layer also owns the product-level split between:
- session ownership;
- subscription ownership.

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

### Session snapshot

The C runtime should expose one snapshot per physical session with:
- session identity;
- endpoint identity;
- runtime state;
- connection generation;
- discovery status;
- transport/association diagnostics;
- reconnect lifecycle flags.

The session snapshot must not be duplicated once per subscription.

### Subscription snapshot

The C runtime should expose one snapshot per report-control subscription with:
- subscription identity;
- session identity;
- endpoint identity;
- group identity;
- report-control reference;
- data-set reference;
- subscription state;
- report-health;
- last report time;
- diagnostics.

One physical session may own many subscription snapshots.

### Verification target input

The Python layer should build a verification target object before planning.

Required fields:
- `signal_id` or row identity;
- `signal_reference`;
- `signal_path`;
- `endpoint_id`;
- `expected_feedback_path`;
- `timeout_ms`;
- `window_ms`;
- optional source metadata from SCD or discovery.

Top-level verification targets are protocol-neutral. Protocol-specific hints belong in `protocol_metadata`.
For IEC 61850, the metadata may include:
- `ied_name`;
- `access_point_name`;
- `logical_device_inst`;
- `logical_node_name`;
- `data_set_reference`;
- `report_control_reference_hint`;
- `fc`;
- `do_name`;
- `da_name`.

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

### Endpoint resolution policy

The Python layer should resolve the transport endpoint before asking the C runtime to connect.

Resolution order:
1. explicit host/port from the workspace runtime configuration or operator-provided target input;
2. endpoint catalog entry for the selected IED/access point when present;
3. SCD-derived endpoint metadata only as a model hint, not as the sole transport source;
4. discovery metadata for reconciling the connected endpoint, not for inventing a connection address.

Practical rules:
- MMS host/port must be known before a real MMS connect attempt;
- SCD is preferred for model binding when it is present and matches the selected IED/access point;
- discovery is used when SCD is missing, incomplete, or does not contain enough report-control detail;
- if SCD and discovery disagree, keep the transport host from the explicit endpoint source and surface a diagnostic instead of silently rewriting the target;
- discovery should enrich report-control and dataset identity after the transport target exists.

Current product implementation uses a runtime-selection seam in the Python layer:
- `runtime_version="simulator"` routes through the simulator-backed verification adapter;
- `runtime_version="mms"` routes through the MMS endpoint catalog and a client-control-backed MMS wrapper;
- if no catalog is passed explicitly, the backend can load a settings-driven JSON MMS endpoint catalog for auto-run execution;
- the MMS wrapper routes one physical session per endpoint and can manage multiple report-control candidates through client-control-backed per-candidate control services;
- endpoint resolution now reports transport source and model source separately so the run can explain whether the transport address came from an explicit request, settings catalog, or was unavailable, and whether the model binding came from loaded SCD or discovery fallback.

### Test fixture override policy

For automated validation only, the Python layer may remap a real device endpoint to a virtual MMS endpoint.

This override is allowed only when:
- the run is clearly marked as test or regression validation;
- the override is recorded in diagnostics and artifacts;
- production endpoint resolution still remains explicit and catalog-driven.

Validation requests may expose explicit override fields such as:
- `transport_override_host`
- `transport_override_port`

These fields should be treated as validation-only transport selectors, not as the authoritative production endpoint identity.

This is useful for:
- lib-server based smoke tests;
- iDiscover simulator-based smoke tests;
- virtual-substation regression runs;
- C264/BCU-focused validation where the field device identity stays the same but the transport target is swapped for a controlled fixture.

The override must not be treated as the production source of truth for MMS host resolution.

### Report update / evidence input

The C runtime should emit report updates with:
- source session / subscription / generation;
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

The Python layer must not depend on raw MMS PDU or BER internals to make verdict decisions.

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
- execution context.

Output:
- verification targets;
- subscription plan;
- uncovered items with reasons.
- coverage summary.

### Session control service

Input:
- endpoint identity;
- desired session action (`connect`, `discover`, `subscribe`, `reconnect`, `disconnect`).

Output:
- live session state;
- one or more subscription snapshots bound to that session;
- diagnostics;
- current freshness summary;
- endpoint-resolution diagnostics showing requested host, resolved host, and whether model binding came from SCD or discovery fallback.

The session snapshot may also expose a compact transport summary:
- total cached signals;
- live signal count;
- stale signal count;
- unknown-freshness signal count;
- signal update count;
- signal change count;
- stale-generation drop count.

Endpoint-resolution diagnostics should be explicit about the split between transport and model binding:
- transport source answers where the MMS host/port came from;
- model source answers whether the IED/report-control model came from loaded SCD or discovery fallback;
- discovery should not invent transport addresses for production runs.

The subscription snapshot should carry the selected RCB key, data-set reference, report-health summary, and recovery/re-subscription identity.

If available, the subscription snapshot should expose a compact report-health summary:
- `unknown` while no valid report has been observed;
- `live` when the report stream is healthy;
- `degraded` when report health has been marked stale or unhealthy;
- report-health reason text when degraded.

If available, the session snapshot should also expose in-flight operation flags:
- connect;
- discover;
- subscribe;
- reconnect.

## Session vs subscription ownership

### Session

Owns:
- endpoint identity;
- connection generation;
- transport state;
- association state;
- reconnect lifecycle;
- discovery snapshot ownership.

### Subscription

Owns:
- report-control identity;
- data-set identity;
- report stream state;
- report health;
- group linkage;
- selected RCB recovery state.

One session can own many subscriptions. The runtime must not duplicate the physical session snapshot once per subscription. A reconnect of that session must re-establish every subscription owned by it.

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
- Product workflow state must remain in Python/FastAPI, not C.
- Session identity and subscription identity must remain separate in the product contract.
- One endpoint session may carry many subscriptions, but the session snapshot itself must appear once per session.

## Current repository status

This repository currently implements the reusable C runtime and its docs, but not the Python/FastAPI service layer.

The Python application should implement this contract before product-level verification work resumes.
