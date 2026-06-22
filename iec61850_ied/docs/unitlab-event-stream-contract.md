# UnitLab event stream contract

Status: draft event contract for the Python/FastAPI product layer.

This document defines the append-only runtime event stream that should back UI updates, diagnostics, and evidence reconstruction.

## Purpose

The event stream is the canonical source for:
- session lifecycle changes;
- discovery snapshot updates;
- subscription changes;
- report arrivals;
- signal freshness changes;
- diagnostics;
- product-level execution state transitions.

The UI may subscribe to the stream, but it must not invent its own source of truth.

## Event envelope

All events should carry:
- `event_id`
- `event_type`
- `occurred_at`
- `session_id`
- `endpoint_id`
- `generation`
- `payload`
- optional `diagnostic_id`

Example:

```json
{
  "event_id": "evt-1001",
  "event_type": "report_received",
  "occurred_at": "2026-06-22T12:00:00Z",
  "session_id": "sess-12",
  "endpoint_id": "mms:IED1@127.0.0.1:102",
  "generation": 4,
  "diagnostic_id": null,
  "payload": {}
}
```

## Event types

### `session_state_changed`

Raised when the runtime session state changes.

Payload:
- `previous_state`
- `current_state`
- `reason`
- `reconnect_active`
- `discovery_status`
- `subscription_status`
- `report_health`

### `discovery_updated`

Raised when a discovery snapshot changes.

Payload:
- `snapshot_id`
- `logical_device_count`
- `logical_node_count`
- `data_set_count`
- `report_control_count`
- `signal_count`
- `status`
- `partial_reason`

### `subscription_updated`

Raised when plan execution changes subscription state.

Payload:
- `report_control_reference`
- `report_control_name`
- `data_set_reference`
- `owner`
- `reserved_by`
- `rptena_enabled`
- `gi_requested`
- `state`
- `reason`

### `report_received`

Raised when an MMS report arrives and is accepted into runtime state.

Payload:
- `rpt_id`
- `data_set_reference`
- `reason`
- `sequence_number`
- `subsequence_number`
- `value_count`
- `matched_value_count`
- `unmatched_value_count`
- `freshness`

### `signal_updated`

Raised when a signal cache entry changes meaningfully.

Payload:
- `signal_id`
- `signal_path`
- `value`
- `quality`
- `timestamp_summary`
- `source_report`
- `freshness`
- `change_flags`

### `signal_stale`

Raised when a live signal becomes stale due to session loss, reconnect, or invalid generation.

Payload:
- `signal_id`
- `signal_path`
- `stale_reason`
- `stale_at`
- `stale_generation`

### `diagnostic_raised`

Raised when a structured diagnostic is added.

Payload:
- `diagnostic_id`
- `scope`
- `category`
- `code`
- `severity`
- `message`
- `details`

### `execution_state_changed`

Raised when the product-level verification execution advances.

Payload:
- `previous_state`
- `current_state`
- `reason`
- `verdict`
- `evidence_count`

## Ordering rules

- Events should be append-only.
- Events should be ordered per session and per generation.
- Late events from an older generation must not be emitted as current-state updates.
- If an old-generation event is retained for diagnostics, it must be clearly labeled as stale or rejected.

## UI consumption rules

- UI may render the latest snapshot, but it should derive that snapshot from events or a service state view.
- UI should not infer state transitions that were not emitted.
- UI should surface stale and failed states explicitly.

## Persistence expectations

- Events should be suitable for later durable storage.
- A reconstructed test run should be explainable from the event stream plus evidence records.
- Do not overload the event stream with ad hoc debug text; keep structured fields for real state.

## Suggested Python modules

- `app/runtime/events.py`
- `app/runtime/session_views.py`
- `app/runtime/sessions.py`
- `app/runtime/evidence.py`

