# UnitLab JSON schema contract

Status: draft canonical field contract for the Python/FastAPI product layer.

This document standardizes the JSON field names used by:
- planning requests and responses;
- session snapshots;
- execution state;
- diagnostics;
- event stream payloads;
- evidence records.

The goal is consistency, not a full formal JSON Schema document.

## Naming rules

- Use `snake_case` in API and event payloads.
- Use stable identifiers, not UI labels, as primary keys.
- Prefer explicit nullable fields over omitted fields when a value is meaningfully absent.
- Preserve source identity and generation everywhere it matters.

## Common fields

These fields should appear consistently wherever applicable:

- `id`
- `session_id`
- `endpoint_id`
- `generation`
- `state`
- `status`
- `reason`
- `code`
- `message`
- `created_at`
- `updated_at`
- `occurred_at`
- `source_generation`
- `diagnostic_id`

## Canonical objects

### 1. `verification_target`

Required:
- `signal_id`
- `signal_reference`
- `signal_path`
- `endpoint_id`
- `ied_name`
- `access_point_name`
- `logical_device_inst`
- `logical_node_name`
- `expected_feedback_path`
- `timeout_ms`
- `window_ms`

Optional:
- `source_row_index`
- `data_set_reference`
- `report_control_reference_hint`
- `source_kind`
- `source_reason`

### 2. `subscription_plan`

Required:
- `plan_id`
- `targets`
- `groups`
- `uncovered_targets`

Group object required fields:
- `group_id`
- `endpoint_id`
- `ied_name`
- `access_point_name`
- `report_control_reference`
- `report_control_name`
- `report_kind`
- `rpt_id`
- `data_set_reference`
- `target_indexes`
- `reason`

Uncovered object required fields:
- `target_index`
- `reason`
- `detail`

### 3. `session_snapshot`

Required:
- `session_id`
- `endpoint_id`
- `state`
- `generation`
- `discovery_status`
- `subscription_status`
- `report_health`

Optional:
- `last_error`
- `last_report_at`
- `selected_report_control`
- `selected_data_set`
- `current_rptena_owner`
- `stale_signal_count`
- `diagnostic_code`

### 4. `signal_state`

Required:
- `signal_id`
- `signal_path`
- `value`
- `quality`
- `freshness`
- `source_session_id`
- `source_endpoint_id`
- `source_generation`

Optional:
- `timestamp_summary`
- `observed_at`
- `stale_reason`
- `stale_at`
- `update_count`
- `change_count`

### 5. `signal_verification_evidence`

Required:
- `evidence_id`
- `signal_id`
- `expected_path`
- `actual_report_path`
- `source_ied`
- `endpoint_id`
- `rpt_id`
- `dataset`
- `received_at`
- `latency_ms`
- `quality`
- `freshness`
- `verdict`
- `reason`

Optional:
- `source_generation`
- `report_reason`
- `signal_value`
- `timestamp_summary`
- `diagnostic_id`

### 6. `diagnostic`

Required:
- `diagnostic_id`
- `scope`
- `category`
- `code`
- `severity`
- `message`
- `created_at`

Optional:
- `session_id`
- `endpoint_id`
- `generation`
- `phase`
- `report_control`
- `data_set`
- `signal_id`
- `details`

### 7. `runtime_event`

Required:
- `event_id`
- `event_type`
- `occurred_at`
- `session_id`
- `endpoint_id`
- `generation`
- `payload`

Optional:
- `diagnostic_id`

## Consistency rules

- `session_id` should identify the product/runtime session, not the physical IED.
- `endpoint_id` should identify the connection target, not the human display label.
- `generation` should advance when transport/session ownership changes.
- `signal_id` should stay stable across runtime refreshes.
- `evidence_id` should be unique per observed confirmation record.
- `diagnostic_id` should be unique per structured diagnostic.

## Error payload shape

Errors should use a stable envelope:

```json
{
  "error": {
    "code": "DISCOVERY_FAILED",
    "message": "Discovery could not complete for the selected endpoint.",
    "scope": "discovery",
    "session_id": "sess-12",
    "endpoint_id": "mms:IED1@127.0.0.1:102",
    "diagnostic_id": "diag-44"
  }
}
```

## Suggested Python modules

- `app/runtime/contracts.py`
- `app/runtime/events.py`
- `app/runtime/evidence.py`
- `app/planning/contracts.py`

