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
`unit_id` names the bound device or allocation target.
`endpoint_id` names the runtime connection endpoint used by planner/runtime/session records and should not repeat the human device id.

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

`endpoint_context` should carry transport identity when the caller already knows the real MMS target. It may include:
- `endpoint_id`
- `host`
- `port`
- `ied_name`
- `access_point_name`
- `scl_path`
- `discovery_snapshot_id`

Transport identity should come from the endpoint context or an endpoint catalog, not from signal-list rows.
`scd_hints` are preferred for model binding when a loaded SCD is available.
`discovery_hints` are fallback model hints when SCD is missing or incomplete.

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
- `transport_override_host`
- `transport_override_port`
- `created_at`
- `triggered_at`

Example:

```json
{
  "signal_id": 1842,
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
- `planning_diagnostics`
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
- `source_classification`
- `source_reason`

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
Use the following precedence when model sources compete:
- exact SCD match;
- discovery match;
- fallback;
- not found.

SCD should remain the first choice for model binding when it exists and matches the selected IED/access-point identity.
Discovery should only fill in missing model detail after the transport target is already known.

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
      "reason": "exact dataset match",
      "source_classification": "from SCD",
      "source_reason": "SCD hint match"
    }
  ],
  "planning_diagnostics": [
    "normalized 3 verification targets",
    "built 1 subscription groups across 1 endpoints"
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

## 2.3 PlannerConfidenceReport

Represents the pre-runtime confidence view for a subscription plan.

Required fields:
- `plan_id`
- `total_targets`
- `covered_targets`
- `partially_covered_targets`
- `uncovered_targets`
- `coverage_percentage`
- `confidence_percentage`
- `groups_count`
- `endpoints_count`
- `planning_quality`
- `risk_level`
- `source_classification_counts`
- `signals`
- `diagnostics`

Each signal entry should include:
- `signal_index`
- `signal_id`
- `signal_reference`
- `endpoint_id`
- `expected_feedback_path`
- `report_control_reference`
- `report_control_name`
- `data_set_reference`
- `coverage_state`
- `source_classification`
- `confidence_state`
- `diagnostics`

Planner confidence is a pre-runtime validation view. It should make fallback bindings, uncovered signals, and missing report-control or dataset context visible before any subscribe or trigger action runs.

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
- `observed_at`
- `latency_ms`
- `quality`
- `evidence_status`
- `reason_code`

Report-derived fields may be null when the evidence represents timeout, invalid, or stale conditions.

Optional fields:
- `session_id`
- `subscription_id`
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

`verification_confidence` is intentionally not stored on the evidence record by default.
Confidence is a derived step/run/explanation property that should be computed from evidence provenance, runtime health, and planning exactness.
If a transitional payload includes `session_id` or `subscription_id`, consumers should treat those as provenance fields, not as a reason to infer session state from evidence alone.

## 3.1 VerificationRunEvidenceResponse

Represents the persisted evidence view for one verification run.

Required fields:
- `test_run_id`
- `evidence_set`
- `evidence_rows`
- `verification_steps`
- `diagnostics`

This response is a read view over persisted evidence rows and evidence-set summary, not a new execution request.

## 3.2 VerificationRunStepDetails

Represents the step-level projection derived from persisted evidence.

Required fields:
- `test_run_id`
- `verification_steps`
- `diagnostics`

This is a read-only inspection shape for operator and API consumers.

### Verification confidence model

`verification_confidence` answers "how strong is the proof?" and must stay separate from `verdict_state`, which answers "did it pass?".

Allowed values:
- `exact_iec61850`
- `exact_report_match`
- `discovery_match`
- `simulated_fallback`
- `simulated`
- `degraded`
- `unknown`

`confidence_reason` should be a normalized code, not ad-hoc prose.
Recommended codes:
- `exact_report_control_match`
- `exact_dataset_match`
- `discovery_match`
- `fallback_planning_used`
- `simulator_generated_report`
- `degraded_recovery_state`
- `partial_coverage`
- `unknown`

Aggregation policy for future multi-signal / multi-IED runs:
- step confidence is derived from the strongest applicable evidence/source classification for that step;
- run confidence is the weakest confidence among the contributing steps after runtime-health modifiers are applied; ties are broken deterministically by `signal_id` and then `step_id`;
- `simulated_fallback` on any contributing step caps the run confidence at `simulated_fallback`;
- `degraded` on any contributing step or session caps the run confidence at `degraded`;
- a `pass` verdict does not increase confidence by itself;
- identical inputs must produce identical confidence output.

Example:

```json
{
  "verdict_state": "pass",
  "verification_confidence": "simulated_fallback",
  "confidence_reason": "fallback_planning_used"
}
```

## 3.3 VerificationAutoRunStart

Represents a single-signal auto verification request.

Required fields:
- `signal_ids`
- `execution_context`

Optional fields:
- `client_id`
- `test_run_id`

The backend should normalize the selected signal-list row(s), execute the run, persist the evidence trail, and return a run snapshot with an explanation.

## 3.4 VerificationVerdictExplanation

Represents the product-facing explanation for why a verification run passed or failed.

Required fields:
- `test_run_id`
- `verdict_state`
- `verification_confidence`
- `confidence_reason`
- `headline`
- `summary`
- `signals`
- `diagnostics`

Signal-level explanation fields should include:
- `signal_id`
- `signal_reference`
- `signal_path`
- `expected_path`
- `observed_path`
- `source_ied`
- `endpoint_id`
- `rpt_id`
- `dataset`
- `evidence_status`
- `verdict_state`
- `latency_ms`
- `reason`
- `diagnostics`

The explanation-level confidence should summarize the run-level proof strength and should not be duplicated as verdict text.

## 3.5 VerificationRunDetailResponse

Represents the persisted run snapshot plus the derived verdict explanation.

Required fields:
- `test_run_id`
- `verification_run`
- `verdict_explanation`

`signal_path` is the canonical stable path used by the product layer.
Fields derived from the observed report may be null for timeout, invalid, or stale evidence.
`actual_report_path` should remain the raw observed report path when one exists.
`evidence_status` describes what was observed.
`verdict_state` is intentionally not stored on the evidence record because it is derived from evidence and policy.
`verification_confidence` belongs to the run/step/explanation view, not the raw evidence record.

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
  "signal_id": 1842,
  "signal_path": "KINTE13LVC01PROT/CT50PTOC1.Op",
  "expected_path": "KINTE13LVC01PROT/CT50PTOC1$ST$Op$general",
  "actual_report_path": "KINTE13LVC01PROT/CT50PTOC1$ST$Op$general",
  "source_ied": "KINTE13LVC01",
  "endpoint_id": "mms:KINTE13LVC01@172.16.40.128:12447",
  "rpt_id": "KINTE13LVC01CTRL/LLN0.brcbA",
  "dataset": "KINTE13LVC01CTRL/LLN0.RCB1",
  "observed_at": "2026-06-22T12:00:00Z",
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

Represents the physical runtime session returned by the reusable IEC 61850 layer.

`runtime_state` is the session lifecycle state and must not be confused with workflow, evidence, or verdict state.
`last_error` is optional; a healthy live session may have no current error.
`SessionSnapshot` should represent one physical session once, even when that session carries multiple subscriptions.
Report-control, dataset, and report-health fields belong in `SubscriptionSnapshot`, not in `SessionSnapshot`.

Required fields:
- `session_id`
- `endpoint_id`
- `runtime_state`
- `connection_generation`
- `discovery_status`

Optional fields:
- `transport_state`
- `association_state`
- `last_error`
- `diagnostic_code`
- `subscription_count`
- `healthy_subscription_count`
- `degraded_subscription_count`

## 4.1 SubscriptionSnapshot

Represents one report-control / data-set subscription inside a physical session.

One `SessionSnapshot` can own many `SubscriptionSnapshot` records.

Required fields:
- `subscription_id`
- `session_id`
- `endpoint_id`
- `group_id`
- `report_control_reference`
- `report_control_name`
- `data_set_reference`
- `subscription_state`
- `report_health`
- `last_report_at`

Optional fields:
- `selected_signal_ids`
- `report_kind`
- `rpt_id`
- `current_rptena_owner`
- `stale_signal_count`
- `last_error`
- `diagnostic_code`
- `diagnostics`

## 5. VerificationRun

Represents one product-level auto verification execution.

`workflow_state`, `runtime_state`, and `verdict_state` must stay separate.
`verification_confidence` and `confidence_reason` are separate from verdict and may default to `unknown` until the confidence classifier is implemented.
For multi-IED runs, `session_snapshots` are the source of truth for transport, `subscription_snapshots` are the source of truth for report streams, and `runtime_state` is only an optional aggregate summary.
If present, `runtime_summary` should be derived from the session and subscription snapshots rather than replace them.
`session_snapshots.length` and `subscription_snapshots.length` are intentionally independent.
Typical runtime-summary fields include session counts, subscription counts, evidence counts, and derived health flags, but not the raw snapshots themselves.

Required fields:
- `test_run_id`
- `verification_targets`
- `subscription_plan`
- `session_snapshots`
- `subscription_snapshots`
- `evidence_set`
- `execution_context`
- `workflow_state`
- `verdict_state`
- `verification_confidence`
- `confidence_reason`

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
`verification_confidence` and `confidence_reason` are separate from verdict and may default to `unknown` until the confidence classifier is implemented.
`VerificationStep` should reference both the physical session and the subscription that owned the report stream.

Required fields:
- `step_id`
- `signal_id`
- `target_index`
- `session_id`
- `subscription_id`
- `step_state`
- `expected_path`
- `expected_window_ms`
- `freshness`
- `evidence_status`
- `verdict_state`
- `evidence_ids`
- `verification_confidence`
- `confidence_reason`

Optional fields:
- `actual_report_path`
- `group_id`
- `source_generation`
- `source_report_rpt_id`
- `source_report_dat_set`
- `source_session_id`
- `triggered_at`
- `observed_at`
- `latency_ms`
- `reason`
- `diagnostics`

`session_id` and `subscription_id` should carry the physical session and the subscription that produced the step.
`source_session_id` is a transitional alias for older payloads and should not be used to infer subscription identity.
`source_session_id` should carry the full runtime session identity, while `group_id` keeps the planner/runtime group binding separate from the session identity.
`verification_confidence` and `confidence_reason` on a step should reflect the strongest proof available for that step, not the overall run verdict.

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

## 8. Session / Subscription examples

### Single IED

One physical session, two subscriptions, ten signals.

```json
{
  "session_snapshots": [
    {
      "session_id": "vr-100:sim:DO-002/unknown",
      "endpoint_id": "sim:DO-002/unknown",
      "runtime_state": "reporting",
      "connection_generation": 1,
      "discovery_status": "available"
    }
  ],
  "subscription_snapshots": [
    {
      "subscription_id": "sub-1",
      "session_id": "vr-100:sim:DO-002/unknown",
      "endpoint_id": "sim:DO-002/unknown",
      "group_id": "group-1",
      "report_control_reference": "kint_4",
      "report_control_name": "kint_4",
      "data_set_reference": "kint_4",
      "subscription_state": "reporting",
      "report_health": "healthy",
      "last_report_at": "2026-06-24T10:11:13.070000Z"
    },
    {
      "subscription_id": "sub-2",
      "session_id": "vr-100:sim:DO-002/unknown",
      "endpoint_id": "sim:DO-002/unknown",
      "group_id": "group-2",
      "report_control_reference": "kint_5",
      "report_control_name": "kint_5",
      "data_set_reference": "kint_5",
      "subscription_state": "reporting",
      "report_health": "healthy",
      "last_report_at": "2026-06-24T10:11:13.070000Z"
    }
  ],
  "verification_steps_count": 10
}
```

### Multi IED

Three physical sessions, six subscriptions, fifty signals.

```json
{
  "session_snapshots": [
    { "session_id": "vr-200:sim:IED-A/P1", "endpoint_id": "sim:IED-A/P1", "runtime_state": "reporting", "connection_generation": 2, "discovery_status": "available" },
    { "session_id": "vr-200:sim:IED-B/P1", "endpoint_id": "sim:IED-B/P1", "runtime_state": "reporting", "connection_generation": 1, "discovery_status": "available" },
    { "session_id": "vr-200:sim:IED-C/P1", "endpoint_id": "sim:IED-C/P1", "runtime_state": "reporting", "connection_generation": 1, "discovery_status": "available" }
  ],
  "subscription_snapshots": [
    { "subscription_id": "sub-a1", "session_id": "vr-200:sim:IED-A/P1", "group_id": "group-a1" },
    { "subscription_id": "sub-a2", "session_id": "vr-200:sim:IED-A/P1", "group_id": "group-a2" },
    { "subscription_id": "sub-b1", "session_id": "vr-200:sim:IED-B/P1", "group_id": "group-b1" },
    { "subscription_id": "sub-b2", "session_id": "vr-200:sim:IED-B/P1", "group_id": "group-b2" },
    { "subscription_id": "sub-c1", "session_id": "vr-200:sim:IED-C/P1", "group_id": "group-c1" },
    { "subscription_id": "sub-c2", "session_id": "vr-200:sim:IED-C/P1", "group_id": "group-c2" }
  ],
  "verification_steps_count": 50
}
```

These examples are intentional:
- `session_snapshots` count and `subscription_snapshots` count are not expected to match;
- one physical session should appear once;
- subscriptions should fan out under the session that owns them.

## Contract rules

- A `VerificationTarget` must exist before a `SubscriptionPlan`.
- A `SubscriptionPlan` must exist before MMS execution.
- A `SignalVerificationEvidence` must be derived from observed runtime data, not guessed in the UI.
- A `VerificationRun` should bind the target, plan, session snapshots, subscription snapshots, and evidence set into one traceable product object.
- A `VerificationStep` should remain traceable to one selected signal, one session, one subscription, and one expected feedback path.
- A `RecoveryState` should preserve desired work and evidence while recovery is in flight.
- `SessionSnapshot` should describe physical session state, not report-control streams or product verdicts.
- `SubscriptionSnapshot` should describe report-control stream state, not physical transport state.
- `source_kind` and `reason` should stay explicit, even for fallback cases.
- `evidence_status` and `verdict_state` must remain separate fields.
- `session_id` and `subscription_id` should not be inferred from selected report-control names.
- `verification_confidence` must remain separate from `verdict_state` and should not be inferred from free-text summary strings.
- `protocol_metadata` may evolve per protocol without changing the top-level target contract.

## Recommended Python modules

- `app/planning/targets.py`
- `app/planning/subscriptions.py`
- `app/runtime/evidence.py`
- `app/runtime/sessions.py`
- `app/runtime/contracts.py`
