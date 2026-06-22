# UnitLab signal-planning contract

Status: draft contract for the Python/FastAPI product layer.

This document defines the boundary between:
- reusable IEC 61850 C primitives in this repository;
- UnitLab product planning in the Python/FastAPI application layer.

## Ownership boundary

### C / IEC 61850 core

The C code in this repository owns:
- MMS association and session lifecycle primitives;
- discovery snapshots and discovered model data;
- report decoding and report-health state;
- signal freshness / stale-generation protection;
- report-control metadata and low-level reusable model references.

The C layer does not own:
- signal-list UX;
- verification target planning;
- per-IED subscription grouping policy;
- verdict/evidence policy;
- product-specific test orchestration.

### Python / FastAPI product layer

The Python application owns:
- signal-list selection;
- target normalization;
- subscription planning;
- reconciliation of desired test intent;
- verdict/evidence generation;
- product-level API contracts and persistence.

## Planned product flow

1. User selects one or more signal-list rows.
2. Python normalizes the selection into verification targets.
3. Python groups targets into per-IED / per-RCB subscription plans.
4. Python asks the reusable IEC 61850 runtime to connect/discover/subscribe.
5. Reports are mapped back into signal confirmation evidence.
6. The test verdict is derived from evidence, timing, and freshness.

## Verification target contract

The Python layer should treat each selected row as a stable verification target with:
- `signal_id` or source row identity;
- `signal_reference` from the signal list;
- `signal_path` as the canonical stable path;
- IEC 61850 endpoint identity;
- logical device / logical node identity;
- expected feedback path;
- report/data-set hint if available;
- timeout/window policy;
- source row index or equivalent traceable identity.

### Required invariants

- One selected signal row must map to one target record before grouping.
- Source identity must be preserved even when later grouped into one subscription plan.
- UI state must not be the source of truth for planning.
- SCD may be a hint, not a hard dependency.

## Subscription plan contract

The Python planner should output a deterministic plan with:
- per-IED or per-endpoint groups;
- chosen report-control candidate;
- reason for the choice;
- uncovered targets with explicit reasons.

Suggested source labels:
- `from SCD`
- `from discovery`
- `fallback`
- `not found`

### Required plan behavior

- exact dataset/report-control match is preferred;
- discovery-derived candidates may be used when available;
- fallback must be explicit and deterministic;
- uncovered targets must not disappear;
- planner must not execute MMS writes directly.

## Evidence contract

The Python layer should create an evidence record for each confirmed feedback path with:
- target identity;
- chosen endpoint / IED;
- report control identifier;
- data set identifier;
- received timestamp;
- latency / timing window result;
- freshness / stale status;
- verdict;
- reason.

The evidence record should be reconstructable without relying on transient UI state.

## Minimal data exchange with C

The Python layer should be able to request from C:
- discovered logical devices / nodes / data sets / report controls;
- selected report-control metadata;
- report summary and live signal updates;
- freshness / stale state;
- safe connect/discover/subscribe/reconnect actions.

The C layer should not need to understand product-level verdict semantics.

## Current implementation gap

This repository currently provides the reusable C runtime and discovery/report primitives, but not the Python implementation of the planning contract.

That means the next product-layer PRs should live in the FastAPI app repository and implement:
- `verification target` normalization;
- `subscription plan` building;
- `evidence` creation;
- `verdict` computation.

## Recommended next slices in Python

1. Signal-list row normalization into verification targets.
2. Subscription planner from verification targets to per-IED groups.
3. Evidence model and verdict states.
4. First auto-confirmed test flow.
5. Multi-IED group handling.

## Validation expectations

- Planning should be deterministic for the same signal-list input.
- Uncovered targets should remain visible.
- No planner step should mutate the IEC 61850 core runtime directly.
- C runtime tests should continue to cover discovery, session, report, freshness, and reconnect behavior independently of product planning.

