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
- expected feedback path;
- timeout / timing window policy.

Top-level verification-target fields should be protocol-neutral.
Protocol-specific hints belong in a `protocol_metadata` object.

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

Discovery is a model source, not a transport source.
It can enrich or validate an endpoint that is already known, but it does not invent the MMS host/IP for the first connect.

### SCD

An SCL/SCD file or imported SCD snapshot used as the preferred source of IEC 61850 model binding.

When present and complete, SCD should be used first for:
- report-control naming;
- dataset binding;
- logical device / logical node identity;
- exact feedback-path reconstruction.

If SCD is missing or incomplete, discovery may be used as a fallback model source after the transport endpoint is known.

### MMS endpoint catalog

The explicit runtime mapping from IED/access-point identity to host/port.

This is the authoritative source for transport reachability in real MMS flows.
In validation flows, a loaded SCD may also be parsed into a transport catalog for the selected IED/access-point when it contains ConnectedAP/IP metadata.
A validation-only transport override may then remap the final host/port for virtual-substation runs.
That derived view is still separate from the operator-configured MMS endpoint catalog, the SCD model source, and discovery metadata.

### Subscription plan

The per-IED / per-RCB execution plan produced from verification targets.

It tells the runtime what to connect to and what to enable.

### Execution context

The source inputs and policy versions used to produce a verification run.

It should preserve:
- project identity;
- signal-list revision;
- selected group identity;
- discovery or SCD snapshot identity when available;
- planner version;
- runtime version;
- policy version;
- operator identity when available;
- timestamps.

### Session

The runtime-owned physical connection state for one endpoint/device identity.

It includes transport state, association state, discovery snapshot, connection generation, and diagnostics.
It should not carry per-report-control execution state once the contract separates subscriptions from sessions.

Current transitional payloads may still expose session+subscription hybrid snapshots, but the target contract treats that as legacy shape.

### Session snapshot

The serialized product or runtime view of one physical session.

A session snapshot should expose:
- `session_id`;
- `endpoint_id`;
- session lifecycle and transport state;
- connection generation;
- discovery state;
- transport/association diagnostics.

It should not duplicate once per subscription.

### Subscription

The runtime-owned report-control execution state inside one session.

A subscription owns:
- report-control identity;
- data-set identity;
- report stream state;
- report health;
- group linkage;
- per-subscription diagnostics.

One session can own many subscriptions.

### Connection generation

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

### Evidence status

The observation state of a single evidence record.

Typical values:
- `none`
- `observed`
- `stale`
- `timeout`
- `invalid`
- `late`
- `out_of_window`

### Evidence

A durable record that a report, signal update, or timing window observation occurred.

Evidence preserves provenance and should survive reconnects and stale transitions.

### Verification run

The product-level container for one auto verification attempt.

It binds:
- selected verification targets;
- the subscription plan used for execution;
- session snapshots;
- subscription snapshots;
- the durable evidence set;
- an optional runtime summary aggregate;
- the final verdict state.
It should also carry a verification confidence level and reason that describe proof strength independently from verdict.

### Workflow state

The product-level execution lifecycle for a verification run.

Typical values:
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

### Runtime state

The session lifecycle state returned by the reusable runtime.

Typical values:
- `connecting`
- `discovering`
- `subscribing`
- `reporting`
- `reconnecting`
- `degraded`
- `closed`

### Verification step

A single executable observation inside a verification run.

It remains traceable to:
- one selected signal row;
- one expected feedback path;
- one runtime session;
- one runtime subscription;
- one runtime observation or timeout outcome.
It may accumulate multiple evidence records over time.

### Verification step state

The lifecycle state for one executable verification step.

It should remain separate from workflow, runtime, evidence, and verdict state.

### Recovery state

The observable product state while reconnect or recovery is in progress.

It preserves:
- desired work;
- active connection generation;
- preserved evidence;
- stale counts and diagnostics.

### Runtime summary

An optional aggregate view of runtime health across session and subscription snapshots.

It should be derived from the underlying snapshots rather than replace them.

### Verdict state

The product-level conclusion for a verification step or run, such as:
- `pending`
- `pass`
- `fail`
- `inconclusive`
- `aborted`

### Verification confidence

The proof-strength classification for a verification step or run.

Typical values:
- `exact_iec61850`
- `exact_report_match`
- `discovery_match`
- `simulated_fallback`
- `simulated`
- `degraded`
- `unknown`

Confidence answers "how strong is the proof?" and must remain separate from verdict state.
Do not confuse it with planner confidence, which is a pre-runtime validation view over target coverage and binding quality.

### Confidence reason

A normalized reason code that explains why the current confidence level was assigned.

Examples:
- `exact_report_control_match`
- `exact_dataset_match`
- `discovery_match`
- `fallback_planning_used`
- `simulator_generated_report`
- `degraded_recovery_state`
- `partial_coverage`
- `unknown`

### Subscription snapshot

The runtime-owned execution state for one report-control / data-set stream inside one session.

It should include:
- `subscription_id`
- `session_id`
- `group_id`
- `endpoint_id`
- `report_control_reference`
- `report_control_name`
- `data_set_reference`
- `subscription_state`
- `report_health`
- `last_report_at`
- `diagnostics`

One session can own many subscription snapshots.

### Subscription state

The lifecycle state for one report-control subscription inside a session.

Typical values:
- `pending`
- `reserving`
- `enabled`
- `reporting`
- `reconnecting`
- `degraded`
- `closed`

## Naming rules

- Use `signal_path` for product identity.
- Use `data_reference` for raw protocol references.
- Use `display_reference` only for operator-facing display.
- Use `source_*` fields for provenance.
- Use `snapshot_*` fields for discovery/session snapshots.
- Use `connection_generation` for session generation tokens and `source_generation` for report provenance.
- Use `evidence_status` for observed feedback state.
- Use `verdict_state` for product-level decision state.
- Use `verification_confidence` for proof strength and `confidence_reason` for the normalized reason code.

## Boundary rules

- The reusable IEC 61850 runtime may emit `data_reference`, snapshot, connection_generation, freshness, and diagnostics.
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
