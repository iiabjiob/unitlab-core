# UnitLab API payload shapes

Status: draft transport-agnostic payload shapes for the Python/FastAPI product layer.

This document narrows the signal-planning and runtime contracts into concrete objects that the app layer can move around. The transport can still be REST, RPC, queue, or internal service calls.

## 1. VerificationTarget

Represents one selected signal-list row before grouping.

Top-level fields are protocol-neutral so the same target model can support IEC 61850 now and other protocols later.

Required fields:
- `signal_id`
- `signal_reference`
- `signal_path`
- `endpoint_id`
- `expected_feedback_path`
- `timeout_ms`
- `window_ms`

Optional fields:
- `source_row_index`
- `source_kind` (`scd`, `discovery`, `fallback`, `unknown`)
- `source_reason`
- `protocol`
- `protocol_metadata`

`protocol_metadata` may contain protocol-specific hints such as IEC 61850 endpoint and model details, but the planner and verdict engine must not require them at the top level.

For IEC 61850, `protocol_metadata` may include:
- `ied_name`
- `access_point_name`
- `logical_device_inst`
- `logical_node_name`
- `data_set_reference`
- `report_control_reference_hint`
- `fc`
- `do_name`
- `da_name`

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
- `execution_context`

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

## 1.3 ExecutionContext

Represents the source inputs and policy versions used to produce a verification run.

Required fields:
- `project_id`
- `signal_list_revision_id`
- `planner_version`
- `runtime_version`
- `policy_version`

Optional fields:
- `selected_group_id`
- `scd_revision_id`
- `discovery_snapshot_id`
- `operator_id`
- `created_at`
- `triggered_at`

Example:

```json
{
  "signal_id": "row-1842",
  "signal_reference": "PROT/CT50PTOC1.Op[ST]",
  "signal_path": "KINTE13LVC01PROT/CT50PTOC1.Op",
  "endpoint_id": "mms:KINTE13LVC01@172.16.40.128:12447",
  "expected_feedback_path": "KINTE13LVC01PROT/CT50PTOC1$ST$Op$general",
  "timeout_ms": 2500,
  "window_ms": 500,
  "protocol": "iec61850",
  "protocol_metadata": {
    "ied_name": "KINTE13LVC01",
    "access_point_name": "P1",
    "logical_device_inst": "PROT",
    "logical_node_name": "CT50PTOC1",
    "data_set_reference": "KINTE13LVC01PROT/LLN0.RCB1",
    "fc": "ST",
    "do_name": "Op",
    "da_name": "general"
  },
  "source_row_index": 17,
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
- `coverage`

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

Coverage should be explicit and deterministic:
- `total_targets`
- `covered_targets`
- `uncovered_targets`
- `partially_covered_targets`
- `groups_count`
- `endpoints_count`
- `planning_quality`

## 2.1 SubscriptionPlanRequest

Represents the product-layer request to group verification targets into executable subscription plans.

Required fields:
- `verification_targets`

Optional fields:
- `discovery_snapshot`
- `scd_hints`
- `planning_policy`
- `execution_context`

## 2.2 SubscriptionPlanResult

Represents the output of subscription planning before execution.

Required fields:
- `subscription_plan`
- `planning_diagnostics`
- `uncovered_targets`
- `coverage`

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
- `signal_path`
- `expected_path`
- `actual_report_path`
- `source_ied`
- `endpoint_id`
- `rpt_id`
- `dataset`
- `received_at`
- `latency_ms`
- `quality`
- `evidence_status`
- `reason_code`

Optional fields:
- `source_generation`
- `source_report_sequence_generation`
- `source_report_sequence_number`
- `source_report_sub_sequence_number`
- `report_reason`
- `signal_value`
- `timestamp_summary`
- `stale_reason`
- `evidence_kind`
- `diagnostics`

`signal_path` is the canonical stable path used by the product layer.
`actual_report_path` should remain the raw observed report path.
`evidence_status` describes what was observed.
`verdict_state` is intentionally not stored on the evidence record because it is derived from evidence and policy.

## 3.1 SignalVerificationEvidenceSet

Represents the durable evidence collection associated with a test run or selected target group.

Required fields:
- `test_run_id`
- `evidence`
- `summary`
- `diagnostics`

The summary should remain explicit and reconstructable, for example:
- `evidence_count`
- `observed_count`
- `stale_count`
- `timeout_count`
- `invalid_count`
- `late_count`
- `out_of_window_count`
- `source_generation`

Example:

```json
{
  "evidence_id": "ev-901",
  "signal_id": "row-1842",
  "signal_path": "KINTE13LVC01PROT/CT50PTOC1.Op",
  "expected_path": "KINTE13LVC01PROT/CT50PTOC1$ST$Op$general",
  "actual_report_path": "KINTE13LVC01PROT/CT50PTOC1$ST$Op$general",
  "source_ied": "KINTE13LVC01",
  "endpoint_id": "mms:KINTE13LVC01@172.16.40.128:12447",
  "rpt_id": "KINTE13LVC01CTRL/LLN0.brcbA",
  "dataset": "KINTE13LVC01CTRL/LLN0.RCB1",
  "received_at": "2026-06-22T12:00:00Z",
  "latency_ms": 42,
  "quality": "good",
  "evidence_status": "observed",
  "reason_code": "report_received",
  "diagnostics": [
    {
      "code": "report_received",
      "message": "Report received within verification window."
    }
  ]
}
```

## 4. SessionSnapshot

Represents the current runtime view returned by the reusable IEC 61850 layer.

`runtime_state` is the session lifecycle state and must not be confused with workflow, evidence, or verdict state.
`last_error` is optional; a healthy live session may have no current error.

Required fields:
- `session_id`
- `endpoint_id`
- `runtime_state`
- `connection_generation`
- `discovery_status`
- `subscription_status`
- `report_health`
- `last_report_at`

Optional fields:
- `last_error`
- `selected_report_control`
- `selected_data_set`
- `current_rptena_owner`
- `stale_signal_count`
- `diagnostic_code`

## 5. VerificationRun

Represents one product-level auto verification execution.

`workflow_state`, `runtime_state`, and `verdict_state` must stay separate.
For multi-IED runs, `session_snapshots` are the source of truth and `runtime_state` is only an optional aggregate summary.
If present, `runtime_summary` should be derived from the per-session snapshots rather than replace them.

Required fields:
- `test_run_id`
- `verification_targets`
- `subscription_plan`
- `session_snapshots`
- `evidence_set`
- `execution_context`
- `workflow_state`
- `verdict_state`

Optional fields:
- `selected_group_id`
- `operator_id`
- `triggered_at`
- `completed_at`
- `runtime_state`
- `runtime_summary`
- `reason`
- `diagnostics`

## 6. VerificationStep

Represents one executable observation inside a verification run.

`step_state` is the step-local lifecycle state and must not be confused with workflow, runtime, evidence, or verdict state.
`evidence_ids` is an ordered list of evidence record ids and may include multiple observations for the same step, including late, stale, duplicate, or recovered reports.

Required fields:
- `step_id`
- `signal_id`
- `target_index`
- `step_state`
- `expected_path`
- `expected_window_ms`
- `freshness`
- `evidence_status`
- `verdict_state`
- `evidence_ids`

Optional fields:
- `actual_report_path`
- `source_session_id`
- `source_generation`
- `source_report_rpt_id`
- `source_report_dat_set`
- `triggered_at`
- `observed_at`
- `latency_ms`
- `reason`
- `diagnostics`

## 7. RecoveryState

Represents the observable recovery state for a session during reconnect/hardening.

`runtime_state` is the current recovery/session state; `desired_state` is the target product intent.
`preserved_evidence_count` is required because recovery must keep the prior evidence trail visible.

Required fields:
- `session_id`
- `endpoint_id`
- `runtime_state`
- `desired_state`
- `active_generation`
- `recovery_reason`
- `desired_subscription_plan_id`
- `desired_group_ids`
- `desired_report_controls`
- `desired_target_ids`
- `active_verification_run_id`
- `preserved_evidence_count`

Optional fields:
- `in_flight`
- `preserved_verification_targets`
- `stale_signal_count`
- `diagnostics`

## Contract rules

- A `VerificationTarget` must exist before a `SubscriptionPlan`.
- A `SubscriptionPlan` must exist before MMS execution.
- A `SignalVerificationEvidence` must be derived from observed runtime data, not guessed in the UI.
- A `VerificationRun` should bind the target, plan, session snapshots, and evidence set into one traceable product object.
- A `VerificationStep` should remain traceable to one selected signal and one expected feedback path.
- A `RecoveryState` should preserve desired work and evidence while recovery is in flight.
- `SessionSnapshot` should describe runtime state, not product verdicts.
- `source_kind` and `reason` should stay explicit, even for fallback cases.
- `evidence_status` and `verdict_state` must remain separate fields.
- `protocol_metadata` may evolve per protocol without changing the top-level target contract.

## Recommended Python modules

- `app/planning/targets.py`
- `app/planning/subscriptions.py`
- `app/runtime/evidence.py`
- `app/runtime/sessions.py`
- `app/runtime/contracts.py`
