# UnitLab contract glossary

Status: canonical terminology for the Python/FastAPI product layer and the reusable IEC 61850 runtime boundary.

This glossary exists to keep the product contracts consistent across roadmap, API, dataflow, and runtime integration docs.

## Core terms

### Signal list

The persisted or imported set of signals that the user selects for verification.

### Signal row

A single row in the signal list with its own stable row identity.

### Verification target

A normalized product-layer object derived from one selected signal row.

It should include:
- source row identity;
- `signal_id`;
- `signal_path`;
- endpoint / IED identity;
- expected feedback path;
- timeout / timing window policy.

### Signal path

The canonical stable key used by the product layer to identify a signal across planning, evidence, and verdict workflows.

`signal_path` is the primary identity for the product layer cache.

### Data reference

The raw protocol source reference coming from the runtime report or discovery mapping.

`data_reference` is descriptive source metadata, not the primary cache key.

### Display reference

A human-readable/debug reference shown in logs or UI.

`display_reference` must not be used as the primary identity for planning or evidence.

### Discovery snapshot

The runtime-provided snapshot of discovered devices, nodes, datasets, report controls, and summary counts.

Discovery snapshots are versioned or identifiable and should not be mutated into a different identity.

### Subscription plan

The per-IED / per-RCB execution plan produced from verification targets.

It tells the runtime what to connect to and what to enable.

### Session

The runtime-owned connection state for one endpoint/device identity.

It includes desired state, live state, discovery snapshot, generation, diagnostics, and freshness summary.

### Runtime generation

A monotonically changing session generation token used to reject stale report updates from prior connections.

### Signal state

The live cache entry for one signal path.

It stores value, quality, timestamp summary, freshness, provenance, and update/change metadata.

### Freshness

The current validity of a signal or report-derived state.

Typical values:
- `live`
- `stale`
- `unknown`

### Evidence

A durable record that a report, signal update, or timing window observation occurred.

Evidence preserves provenance and should survive reconnects and stale transitions.

### Verification run

The product-level container for one auto verification attempt.

It binds:
- selected verification targets;
- the subscription plan used for execution;
- session snapshots;
- the durable evidence set;
- the final verdict.

### Verification step

A single executable observation inside a verification run.

It remains traceable to:
- one selected signal row;
- one expected feedback path;
- one runtime observation or timeout outcome.

### Recovery state

The observable product state while reconnect or recovery is in progress.

It preserves:
- desired work;
- active generation;
- preserved evidence;
- stale counts and diagnostics.

### Verdict

The product-level conclusion for a verification step or run, such as:
- `confirmed`
- `verified`
- `timed_out`
- `failed`
- `stale`

## Naming rules

- Use `signal_path` for product identity.
- Use `data_reference` for raw protocol references.
- Use `display_reference` only for operator-facing display.
- Use `source_*` fields for provenance.
- Use `snapshot_*` fields for discovery/session snapshots.
- Use `generation` only for runtime generation tokens.

## Boundary rules

- The reusable IEC 61850 runtime may emit `data_reference`, snapshot, generation, freshness, and diagnostics.
- The Python/FastAPI product layer owns `signal_path`, subscription planning, evidence, and verdicts.
- Product-layer naming should stay protocol-independent where possible.

## Related docs

- `docs/iec61850-unitlab-roadmap.md`
- `docs/iec61850-unitlab-readiness-matrix.md`
- `docs/unitlab-product-slice-tracker.md`
- `docs/unitlab-operating-model.md`
- `docs/unitlab-verification-dataflow.md`
- `docs/unitlab-json-schema-contract.md`
- `docs/unitlab-runtime-integration-contract.md`
- `docs/unitlab-api-payload-shapes.md`
