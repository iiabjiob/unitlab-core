# UnitLab API payload shapes

Status: draft transport-agnostic payload shapes for the Python/FastAPI product layer.

This document narrows the signal-planning and runtime contracts into concrete objects that the app layer can move around. The transport can still be REST, RPC, queue, or internal service calls.

## 1. VerificationTarget

Represents one selected signal-list row before grouping.

Required fields:
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

Optional fields:
- `source_row_index`
- `data_set_reference`
- `report_control_reference_hint`
- `source_kind` (`scd`, `discovery`, `fallback`, `unknown`)
- `source_reason`

## 1.1 VerificationTargetNormalizationRequest

Represents the product-layer request to convert selected signal-list rows into verification targets.

Required fields:
- `signal_ids`

Optional fields:
- `endpoint_context`
- `scd_hints`
- `discovery_hints`
- `timeout_policy`
- `window_policy`

## 1.2 VerificationTargetNormalizationResult

Represents the output of signal-list normalization before planning.

Required fields:
- `verification_targets`
- `diagnostics`
- `unresolved_rows`

Each unresolved row should include:
- `signal_id`
- `reason`
- `detail`

Diagnostics should remain explicit and non-fatal when possible.

Example:

```json
{
  "signal_id": "row-1842",
  "signal_reference": "PROT/CT50PTOC1.Op[ST]",
  "signal_path": "KINTE13LVC01PROT/CT50PTOC1.Op",
  "endpoint_id": "mms:KINTE13LVC01@172.16.40.128:12447",
  "ied_name": "KINTE13LVC01",
  "access_point_name": "P1",
  "logical_device_inst": "PROT",
  "logical_node_name": "CT50PTOC1",
  "expected_feedback_path": "KINTE13LVC01PROT/CT50PTOC1$ST$Op$general",
  "timeout_ms": 2500,
  "window_ms": 500,
  "source_row_index": 17,
  "data_set_reference": "KINTE13LVC01PROT/LLN0.RCB1",
  "source_kind": "discovery",
  "source_reason": "derived from discovered dataset membership"
}
```

## 2. SubscriptionPlan

Represents the grouping decision.

Required fields:
- `plan_id`
- `targets`
- `groups`
- `uncovered_targets`

Each group should include:
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

Each uncovered target should include:
- `target_index`
- `reason`
- `detail`

## 2.1 SubscriptionPlanRequest

Represents the product-layer request to group verification targets into executable subscription plans.

Required fields:
- `verification_targets`

Optional fields:
- `discovery_snapshot`
- `scd_hints`
- `planning_policy`

## 2.2 SubscriptionPlanResult

Represents the output of subscription planning before execution.

Required fields:
- `subscription_plan`
- `planning_diagnostics`
- `uncovered_targets`

Each grouped plan item should keep the source classification explicit:
- `from SCD`
- `from discovery`
- `fallback`
- `not found`

Source classification should be stable and deterministic for the same inputs.

Example:

```json
{
  "plan_id": "plan-22",
  "groups": [
    {
      "group_id": "grp-1",
      "endpoint_id": "mms:KINTE13LVC01@172.16.40.128:12447",
      "ied_name": "KINTE13LVC01",
      "access_point_name": "P1",
      "report_control_reference": "KINTE13LVC01/P1/CTRL/LLN0/brcbA/buffered",
      "report_control_name": "brcbA",
      "report_kind": "buffered",
      "rpt_id": "KINTE13LVC01CTRL/LLN0.brcbA",
      "data_set_reference": "KINTE13LVC01CTRL/LLN0.RCB1",
      "target_indexes": [0, 1],
      "reason": "exact dataset match"
    }
  ],
  "uncovered_targets": [
    {
      "target_index": 2,
      "reason": "not found",
      "detail": "no matching report control"
    }
  ]
}
```

## 3. SignalVerificationEvidence

Represents one feedback observation tied to a target.

Required fields:
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

Optional fields:
- `signal_path`
- `source_generation`
- `source_report_sequence_generation`
- `source_report_sequence_number`
- `source_report_sub_sequence_number`
- `report_reason`
- `signal_value`
- `timestamp_summary`
- `stale_reason`
- `evidence_kind`

`signal_path` should be the canonical stable path used by the product layer.
`actual_report_path` should remain the raw observed report path.

## 3.1 SignalVerificationEvidenceSet

Represents the durable evidence collection associated with a test run or selected target group.

Required fields:
- `test_run_id`
- `evidence`
- `summary`
- `diagnostics`

The summary should remain explicit and reconstructable, for example:
- `evidence_count`
- `verified_count`
- `confirmed_count`
- `stale_count`
- `timed_out_count`
- `failed_count`
- `source_generation`

Example:

```json
{
  "evidence_id": "ev-901",
  "signal_id": "row-1842",
  "expected_path": "KINTE13LVC01PROT/CT50PTOC1$ST$Op$general",
  "actual_report_path": "KINTE13LVC01PROT/CT50PTOC1$ST$Op$general",
  "source_ied": "KINTE13LVC01",
  "endpoint_id": "mms:KINTE13LVC01@172.16.40.128:12447",
  "rpt_id": "KINTE13LVC01CTRL/LLN0.brcbA",
  "dataset": "KINTE13LVC01CTRL/LLN0.RCB1",
  "received_at": "2026-06-22T12:00:00Z",
  "latency_ms": 42,
  "quality": "good",
  "freshness": "live",
  "verdict": "verified",
  "reason": "report received within verification window"
}
```

## 4. SessionSnapshot

Represents the current runtime view returned by the reusable IEC 61850 layer.

Required fields:
- `session_id`
- `endpoint_id`
- `state`
- `connection_generation`
- `last_error`
- `discovery_status`
- `subscription_status`
- `report_health`
- `last_report_at`

Optional fields:
- `selected_report_control`
- `selected_data_set`
- `current_rptena_owner`
- `stale_signal_count`
- `diagnostic_code`

## Contract rules

- A `VerificationTarget` must exist before a `SubscriptionPlan`.
- A `SubscriptionPlan` must exist before MMS execution.
- A `SignalVerificationEvidence` must be derived from observed runtime data, not guessed in the UI.
- `SessionSnapshot` should describe runtime state, not product verdicts.
- `source_kind` and `reason` should stay explicit, even for fallback cases.

## Recommended Python modules

- `app/planning/targets.py`
- `app/planning/subscriptions.py`
- `app/runtime/evidence.py`
- `app/runtime/sessions.py`
- `app/runtime/contracts.py`
