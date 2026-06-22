# UnitLab verification dataflow

Status: draft end-to-end dataflow contract for the Python/FastAPI product layer.

This document links the product workflow stages together and keeps the ownership boundary explicit.

## Dataflow

### 1. Signal list

Input:
- selected signal-list rows;
- row identity;
- endpoint metadata when available;
- optional SCD hints.

Output:
- normalized verification targets;
- source row references;
- expected feedback path;
- timeout/window policy.

Owner:
- Python/FastAPI product layer.

### 2. Subscription planning

Input:
- verification targets;
- optional discovery snapshot;
- optional SCD hints.

Output:
- per-IED / per-RCB subscription plan;
- uncovered targets with reasons;
- explicit source classification.

Owner:
- Python/FastAPI product layer.

### 3. Session/runtime execution

Input:
- endpoint identity;
- desired session action;
- report-control / dataset references;
- timing policy.

Output:
- session snapshot;
- discovery snapshot;
- live report state;
- freshness state;
- diagnostics.

Owner:
- reusable IEC 61850 C runtime.

### 4. Report-to-signal mapping

Input:
- runtime report updates;
- source session/generation;
- source report metadata;
- raw value / quality / timestamp summary.

Output:
- signal cache updates;
- signal freshness changes;
- provenance-preserving state.

Owner:
- reusable IEC 61850 C runtime for the raw mapping;
- Python/FastAPI layer for any product-level aggregation or explanation.

### 5. Evidence capture

Input:
- signal cache updates;
- report updates;
- timing window policy;
- source identity.

Output:
- evidence records;
- latency and freshness fields;
- explainable provenance.

Owner:
- Python/FastAPI product layer.

### 6. Verdict computation

Input:
- evidence records;
- test-run state;
- timing window policy;
- operator metadata.

Output:
- confirmed / verified / timed_out / failed / stale verdicts;
- explainable reason codes.

Owner:
- Python/FastAPI product layer.

## Invariants

- The same signal-list input must produce the same normalized target identity.
- Planning must not hide uncovered targets.
- Runtime sessions must preserve source identity and generation.
- Late frames from old generations must not corrupt current evidence.
- Evidence must never erase the original report provenance.
- Verdicts must be computed from evidence, not from UI state.

## Ownership summary

### Reusable IEC 61850 C runtime

- session/connect/disconnect/reconnect primitives;
- discovery snapshots;
- report enablement and report ingestion;
- signal freshness and staleness;
- protocol diagnostics.

### Python/FastAPI product layer

- signal normalization;
- planner;
- evidence model;
- verdict engine;
- persistence;
- API orchestration;
- UI-facing state projection.

## Related docs

- `docs/iec61850-unitlab-roadmap.md`
- `docs/iec61850-unitlab-readiness-matrix.md`
- `docs/unitlab-product-slice-tracker.md`
- `docs/unitlab-operating-model.md`
- `docs/unitlab-runtime-integration-contract.md`
- `docs/unitlab-signal-planning-contract.md`
- `docs/unitlab-api-payload-shapes.md`
- `docs/unitlab-verification-execution-contract.md`
- `docs/unitlab-recovery-contract.md`
- `docs/unitlab-diagnostics-contract.md`

