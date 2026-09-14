from __future__ import annotations

from datetime import UTC, datetime, timedelta
from types import SimpleNamespace

import pytest

from app.schemas.verification_schema import VerificationExecutionContextSchema
from app.services.iec61850.report_runtime import Iec61850DeviceEndpoint, Iec61850RuntimeMode
from app.services.verification_execution import _build_step_and_evidence, _first_non_empty_report_text, _latency_ms, _resolve_evidence_state, build_runtime_subscription_plan, execute_simulated_verification_run
from app.services.verification_planner import VerificationTargetSource, build_verification_subscription_plan


class _FakeVerificationEvidenceRepository:
    def __init__(self) -> None:
        self.rows: list[dict] = []
        self.evidence_sets: list[dict] = []

    async def record_signal_verification_evidence(self, **kwargs):
        self.rows.append(dict(kwargs))
        return SimpleNamespace(**kwargs)

    async def upsert_signal_verification_evidence_set(self, **kwargs):
        self.evidence_sets.append(dict(kwargs))
        return SimpleNamespace(**kwargs)


def _build_plan():
    return build_verification_subscription_plan(
        [
            VerificationTargetSource(
                signal_id=101,
                signal_reference="Breaker Close",
                signal_path="breaker_close",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "ied_name": "IED-A",
                        "access_point_name": "P1",
                        "report_control_reference_hint": "IED-A/P1/LLN0.brA/buffered",
                        "report_control_name": "brA",
                        "report_kind": "buffered",
                        "rpt_id": "IED-A/LLN0.brA",
                        "data_set_reference": "IED-A/LLN0.dsA",
                        "expected_feedback_path": "LD0/XCBR1.Pos.stVal[ST]",
                    },
                },
                allocation_id=1,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=11,
                channel_label="DO-11",
                unit_id="IED-A/P1",
                unit_online=True,
                source_row_id="signal-101",
            )
        ]
    )


def test_report_identifier_normalization_ignores_native_empty_placeholder() -> None:
    assert _first_non_empty_report_text("<empty>", "brcbST01", "fallback") == "brcbST01"
    assert _first_non_empty_report_text("<none>", "fallback") == "fallback"


def test_report_before_trigger_is_not_coerced_to_fresh_zero_latency() -> None:
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)
    observed_at = triggered_at - timedelta(minutes=5)
    latency_ms = _latency_ms(triggered_at, observed_at)

    assert latency_ms == -300_000
    status, freshness, reason, kind = _resolve_evidence_state(
        target=_build_plan().targets[0],
        actual_report_path="LD0/XCBR1.Pos.stVal[ST]",
        observed_at=observed_at,
        signal_value=True,
        report_reason="data-change",
        latency_ms=latency_ms,
        runtime_result=SimpleNamespace(diagnostics=()),
        window_ms=500,
        timeout_ms=2_000,
    )

    assert (status, freshness, reason, kind) == (
        "stale",
        "stale",
        "report_before_trigger",
        "report_observation",
    )


def test_report_with_matched_path_but_missing_value_is_invalid() -> None:
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)
    status, freshness, reason, kind = _resolve_evidence_state(
        target=_build_plan().targets[0],
        actual_report_path="LD0/XCBR1.Pos.stVal[ST]",
        observed_at=triggered_at + timedelta(milliseconds=10),
        signal_value=None,
        latency_ms=10,
        runtime_result=SimpleNamespace(diagnostics=()),
        window_ms=500,
        timeout_ms=2_000,
    )

    assert (status, freshness, reason, kind) == (
        "invalid",
        "unknown",
        "missing_signal_value",
        "report_observation",
    )


def test_non_causal_report_reason_cannot_confirm_trigger() -> None:
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)
    status, freshness, reason, kind = _resolve_evidence_state(
        target=_build_plan().targets[0],
        actual_report_path="LD0/XCBR1.Pos.stVal[ST]",
        observed_at=triggered_at + timedelta(milliseconds=10),
        signal_value=True,
        report_reason="integrity",
        latency_ms=10,
        runtime_result=SimpleNamespace(diagnostics=()),
        window_ms=500,
        timeout_ms=2_000,
    )

    assert (status, freshness, reason, kind) == (
        "invalid",
        "unknown",
        "non_causal_report_reason",
        "report_observation",
    )


def test_source_observation_timestamp_controls_freshness_over_receive_time() -> None:
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)
    source_timestamp = (triggered_at - timedelta(minutes=5)).isoformat()
    report = SimpleNamespace(
        event=SimpleNamespace(
            received_at=(triggered_at + timedelta(milliseconds=10)).isoformat(),
            endpoint_id="IED-A/P1",
            rpt_id="IED-A/LLN0.brA",
            sequence_number=4,
            reason=SimpleNamespace(value="data-change"),
        ),
        ied_name="IED-A",
        report_control_name="brA",
        data_set_ref="IED-A/LLN0.dsA",
    )

    evidence, step = _build_step_and_evidence(
        target_index=0,
        target=_build_plan().targets[0],
        group=_build_plan().groups[0],
        observation_bundle=(report, "LD0/XCBR1.Pos.stVal[ST]", True, source_timestamp),
        triggered_at=triggered_at,
        runtime_result=SimpleNamespace(diagnostics=()),
        test_run_id="run-1",
    )

    assert evidence.observed_at == triggered_at - timedelta(minutes=5)
    assert evidence.evidence_status == "stale"
    assert step.verdict_state != "pass"


def test_observed_evidence_does_not_infer_good_quality() -> None:
    plan = _build_plan()
    report = SimpleNamespace(
        event=SimpleNamespace(
            received_at="2026-06-23T12:00:00.100Z",
            endpoint_id="IED-A/P1",
            rpt_id="IED-A/LLN0.brA",
            sequence_number=2,
            reason=SimpleNamespace(value="data-change"),
        ),
        ied_name="IED-A",
        report_control_name="brA",
        data_set_ref="IED-A/LLN0.dsA",
    )

    evidence, _ = _build_step_and_evidence(
        target_index=0,
        target=plan.targets[0],
        group=plan.groups[0],
        observation_bundle=(report, "LD0/XCBR1.Pos.stVal[ST]", True, "2026-06-23T12:00:00.100Z"),
        triggered_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        runtime_result=SimpleNamespace(diagnostics=()),
        test_run_id="run-quality-unknown",
    )

    assert evidence.evidence_status == "observed"
    assert evidence.quality == "unknown"


def test_runtime_subscription_plan_keeps_transport_endpoint_out_of_ied_name() -> None:
    plan = build_verification_subscription_plan(
        [
            VerificationTargetSource(
                signal_id=101,
                signal_reference="Trip",
                signal_path="trip",
                signal_metadata={
                    "row": {
                        "transport_host": "172.16.40.128:12447",
                        "iec61850_address": "KINTE15BCU01CTRL1/CBCSWI1/Pos/stVal[ST]",
                    },
                    "verification": {
                        "enabled": True,
                        "transport_host": "172.16.40.128:12447",
                        "iec61850_address": "KINTE15BCU01CTRL1/CBCSWI1/Pos/stVal[ST]",
                    },
                },
                allocation_id=None,
                allocation_status="unassigned",
                allocation_health={},
                channel_id=None,
                channel_label=None,
                unit_id=None,
                unit_online=None,
                source_row_id="signal-101",
            )
        ]
    )

    runtime_plan = build_runtime_subscription_plan(plan)

    assert runtime_plan.devices[0].ied_name == ""
    assert runtime_plan.devices[0].endpoint_id == "172.16.40.128:12447"
    assert runtime_plan.devices[0].reports[0].candidate.ied_name == ""


def test_fallback_runtime_plan_groups_signal_list_addresses_by_logical_device_and_fc() -> None:
    plan = build_verification_subscription_plan(
        [
            VerificationTargetSource(
                signal_id=101,
                signal_reference="CTRL1 Pos",
                signal_path="ctrl1_pos",
                signal_metadata={
                    "row": {
                        "transport_host": "172.16.40.128:12447",
                        "iec61850_address": "KINTE15BCU01CTRL1/CBCSWI1/Pos/stVal[ST]",
                    }
                },
                allocation_id=None,
                allocation_status="unassigned",
                allocation_health={},
                channel_id=None,
                channel_label=None,
                unit_id=None,
                unit_online=None,
                source_row_id="signal-101",
            ),
            VerificationTargetSource(
                signal_id=102,
                signal_reference="CTRL1 Ind",
                signal_path="ctrl1_ind",
                signal_metadata={
                    "row": {
                        "transport_host": "172.16.40.128:12447",
                        "iec61850_address": "KINTE15BCU01CTRL1/SlotHGGIO12/Ind15/stVal[ST]",
                    }
                },
                allocation_id=None,
                allocation_status="unassigned",
                allocation_health={},
                channel_id=None,
                channel_label=None,
                unit_id=None,
                unit_online=None,
                source_row_id="signal-102",
            ),
            VerificationTargetSource(
                signal_id=103,
                signal_reference="CTRL2 Ind",
                signal_path="ctrl2_ind",
                signal_metadata={
                    "row": {
                        "transport_host": "172.16.40.128:12447",
                        "iec61850_address": "KINTE15BCU01CTRL2/SlotIGGIO2/Ind1/stVal[ST]",
                    }
                },
                allocation_id=None,
                allocation_status="unassigned",
                allocation_health={},
                channel_id=None,
                channel_label=None,
                unit_id=None,
                unit_online=None,
                source_row_id="signal-103",
            ),
            VerificationTargetSource(
                signal_id=104,
                signal_reference="CTRL1 Measurement",
                signal_path="ctrl1_measurement",
                signal_metadata={
                    "row": {
                        "transport_host": "172.16.40.128:12447",
                        "iec61850_address": "KINTE15BCU01CTRL1/RSYN1/Hz/mag.f[MX]",
                    }
                },
                allocation_id=None,
                allocation_status="unassigned",
                allocation_health={},
                channel_id=None,
                channel_label=None,
                unit_id=None,
                unit_online=None,
                source_row_id="signal-104",
            ),
            VerificationTargetSource(
                signal_id=105,
                signal_reference="CTRL1 Control",
                signal_path="ctrl1_control",
                signal_metadata={
                    "row": {
                        "transport_host": "172.16.40.128:12447",
                        "iec61850_address": "KINTE15BCU01CTRL1/CBCSWI1/Pos/Oper.ctlVal[CO]",
                    }
                },
                allocation_id=None,
                allocation_status="unassigned",
                allocation_health={},
                channel_id=None,
                channel_label=None,
                unit_id=None,
                unit_online=None,
                source_row_id="signal-105",
            ),
            VerificationTargetSource(
                signal_id=106,
                signal_reference="System Health",
                signal_path="system_health",
                signal_metadata={
                    "row": {
                        "transport_host": "172.16.40.128:12447",
                        "iec61850_address": "KINTE15BCU01SYSTEM/LLN0/Health/stVal[ST]",
                    }
                },
                allocation_id=None,
                allocation_status="unassigned",
                allocation_health={},
                channel_id=None,
                channel_label=None,
                unit_id=None,
                unit_online=None,
                source_row_id="signal-106",
            ),
        ]
    )

    runtime_plan = build_runtime_subscription_plan(plan)

    assert plan.coverage.groups_count == 3
    assert plan.coverage.uncovered_targets == 2
    assert runtime_plan.required_report_count == 3
    candidate_signals_by_scope = {
        report.candidate.signals[0].reference.rsplit("/", 3)[0] + "/" + report.candidate.signals[0].reference.rsplit("[", 1)[-1].rstrip("]")
        for device in runtime_plan.devices
        for report in device.reports
    }
    assert candidate_signals_by_scope == {
        "KINTE15BCU01CTRL1/ST",
        "KINTE15BCU01CTRL1/MX",
        "KINTE15BCU01CTRL2/ST",
    }


def _build_multi_ied_plan():
    return build_verification_subscription_plan(
        [
            VerificationTargetSource(
                signal_id=101,
                signal_reference="Breaker Close A",
                signal_path="breaker_close_a",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "ied_name": "IED-A",
                        "access_point_name": "P1",
                        "report_control_reference_hint": "IED-A/P1/LLN0.brA/buffered",
                        "report_control_name": "brA",
                        "report_kind": "buffered",
                        "rpt_id": "IED-A/LLN0.brA",
                        "data_set_reference": "IED-A/LLN0.dsA",
                        "expected_feedback_path": "LD0/XCBR1.Pos.stVal[ST]",
                    },
                },
                allocation_id=1,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=11,
                channel_label="DO-11",
                unit_id="IED-A/P1",
                unit_online=True,
                source_row_id="signal-101",
            ),
            VerificationTargetSource(
                signal_id=202,
                signal_reference="Breaker Close B",
                signal_path="breaker_close_b",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "ied_name": "IED-B",
                        "access_point_name": "P1",
                        "report_control_reference_hint": "IED-B/P1/LLN0.brB/buffered",
                        "report_control_name": "brB",
                        "report_kind": "buffered",
                        "rpt_id": "IED-B/LLN0.brB",
                        "data_set_reference": "IED-B/LLN0.dsB",
                        "expected_feedback_path": "LD0/XCBR2.Pos.stVal[ST]",
                    },
                },
                allocation_id=2,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=21,
                channel_label="DO-21",
                unit_id="IED-B/P1",
                unit_online=True,
                source_row_id="signal-202",
            ),
        ]
    )


def _build_same_ied_plan():
    return build_verification_subscription_plan(
        [
            VerificationTargetSource(
                signal_id=101,
                signal_reference="Breaker Close A",
                signal_path="breaker_close_a",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "ied_name": "IED-A",
                        "access_point_name": "P1",
                        "report_control_reference_hint": "IED-A/P1/LLN0.brA/buffered",
                        "report_control_name": "brA",
                        "report_kind": "buffered",
                        "rpt_id": "IED-A/LLN0.brA",
                        "data_set_reference": "IED-A/LLN0.dsA",
                        "expected_feedback_path": "LD0/XCBR1.Pos.stVal[ST]",
                    },
                },
                allocation_id=1,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=11,
                channel_label="DO-11",
                unit_id="IED-A/P1",
                unit_online=True,
                source_row_id="signal-101",
            ),
            VerificationTargetSource(
                signal_id=102,
                signal_reference="Breaker Close B",
                signal_path="breaker_close_b",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "ied_name": "IED-A",
                        "access_point_name": "P1",
                        "report_control_reference_hint": "IED-A/P1/LLN0.brA/buffered",
                        "report_control_name": "brA",
                        "report_kind": "buffered",
                        "rpt_id": "IED-A/LLN0.brA",
                        "data_set_reference": "IED-A/LLN0.dsA",
                        "expected_feedback_path": "LD0/XCBR2.Pos.stVal[ST]",
                    },
                },
                allocation_id=2,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=12,
                channel_label="DO-12",
                unit_id="IED-A/P1",
                unit_online=True,
                source_row_id="signal-102",
            ),
        ]
    )


def _build_fallback_plan():
    return build_verification_subscription_plan(
        [
            VerificationTargetSource(
                signal_id=13041,
                signal_reference="KINT",
                signal_path="kint_5",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "expected_feedback_path": "IED-ACTRL1/XCBR1.Pos.stVal[ST]",
                    },
                },
                source_row_index=4,
                source_kind=None,
                source_reason=None,
                allocation_id=183399,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": True,
                    "stale_device": False,
                },
                channel_id=3,
                channel_label="CH3",
                unit_id="DO-002",
                unit_online=False,
                source_row_id="signal-13041",
            )
        ]
    )


def _virtual_endpoint_for_plan_device(device) -> Iec61850DeviceEndpoint:
    return Iec61850DeviceEndpoint(
        id=f"sim:{device.ied_name}/{device.access_point_name}@10.10.10.250:12447",
        mode=Iec61850RuntimeMode.SIMULATOR,
        ied_name=device.ied_name,
        access_point_name=device.access_point_name,
        host="10.10.10.250",
        port=12447,
    )


@pytest.mark.anyio
async def test_execute_simulated_verification_run_records_observed_evidence_and_passes() -> None:
    plan = _build_plan()
    repo = _FakeVerificationEvidenceRepository()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)

    result = await execute_simulated_verification_run(
        workspace_id=7,
        test_run_id="run-1",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        repository=repo,  # type: ignore[arg-type]
        triggered_at=triggered_at,
        latency_ms=250,
        now=lambda: triggered_at + timedelta(milliseconds=250),
    )

    assert result.verification_run.workflow_state == "completed"
    assert result.verification_run.verdict_state == "pass"
    assert result.verification_run.recovery_state is not None
    assert result.verification_run.recovery_state.runtime_state == "reporting"
    assert result.verification_run.recovery_state.recovery_reason is None
    assert result.verification_run.verification_confidence == "exact_report_match"
    assert result.verification_run.confidence_reason == "exact_dataset_match"
    assert result.evidence_set.summary.evidence_count == 1
    assert result.evidence_set.summary.observed_count == 1
    assert result.evidence_set.summary.source_generation == 1
    assert result.verification_run.verification_steps[0].evidence_status == "observed"
    assert result.verification_run.verification_steps[0].verdict_state == "pass"
    assert result.verification_run.verification_steps[0].verification_confidence == "exact_report_match"
    assert result.verification_run.verification_steps[0].confidence_reason == "exact_dataset_match"
    assert result.verification_run.verification_steps[0].group_id == "group-1"
    assert result.verification_run.verification_steps[0].source_session_id == "run-1:sim:IED-A/P1"
    assert result.verification_run.verification_steps[0].source_generation == 1
    assert repo.rows[0]["evidence_status"] == "observed"
    assert repo.rows[0]["source_generation"] == 1
    assert repo.evidence_sets[0]["evidence"][0].signal_id == 101


@pytest.mark.anyio
async def test_execute_simulated_verification_run_uses_custom_endpoint_mapper() -> None:
    plan = _build_plan()
    repo = _FakeVerificationEvidenceRepository()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)

    result = await execute_simulated_verification_run(
        workspace_id=7,
        test_run_id="run-custom-endpoint",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        repository=repo,  # type: ignore[arg-type]
        triggered_at=triggered_at,
        latency_ms=250,
        now=lambda: triggered_at + timedelta(milliseconds=250),
        endpoint_for_device=_virtual_endpoint_for_plan_device,
    )

    assert result.verification_run.verdict_state == "pass"
    assert result.verification_run.verification_confidence == "exact_report_match"
    assert result.verification_run.session_snapshots[0].endpoint_id == "sim:IED-A/P1@10.10.10.250:12447"
    assert result.verification_run.subscription_snapshots[0].endpoint_id == "sim:IED-A/P1@10.10.10.250:12447"
    assert result.verification_run.verification_steps[0].source_session_id == "run-custom-endpoint:sim:IED-A/P1@10.10.10.250:12447"
    assert result.evidence_set.evidence[0].endpoint_id == "sim:IED-A/P1@10.10.10.250:12447"


@pytest.mark.anyio
async def test_execute_simulated_verification_run_marks_fallback_planning_as_simulated_fallback() -> None:
    plan = _build_fallback_plan()
    repo = _FakeVerificationEvidenceRepository()
    triggered_at = datetime(2026, 6, 24, 10, 11, 12, 820_000, tzinfo=UTC)

    result = await execute_simulated_verification_run(
        workspace_id=7,
        test_run_id="run-fallback",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=2,
            signal_list_revision_id=2,
            planner_version="ui-auto-run",
            runtime_version="simulator",
            policy_version="v1",
        ),
        repository=repo,  # type: ignore[arg-type]
        triggered_at=triggered_at,
        latency_ms=250,
        now=lambda: triggered_at + timedelta(milliseconds=250),
    )

    assert result.verification_run.verdict_state == "pass"
    assert result.verification_run.verification_confidence == "simulated_fallback"
    assert result.verification_run.confidence_reason == "fallback_planning_used"
    assert result.verification_run.verification_steps[0].verification_confidence == "simulated_fallback"
    assert result.verification_run.verification_steps[0].confidence_reason == "fallback_planning_used"
    assert result.verification_run.recovery_state is not None
    assert result.verification_run.recovery_state.runtime_state == "reporting"


@pytest.mark.anyio
async def test_execute_simulated_verification_run_marks_late_observation_as_fail() -> None:
    plan = _build_plan()
    plan.targets[0].window_ms = 500
    plan.targets[0].timeout_ms = 2000
    repo = _FakeVerificationEvidenceRepository()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)

    result = await execute_simulated_verification_run(
        workspace_id=7,
        test_run_id="run-2",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        repository=repo,  # type: ignore[arg-type]
        triggered_at=triggered_at,
        latency_ms=1500,
        now=lambda: triggered_at + timedelta(milliseconds=1500),
    )

    assert result.verification_run.workflow_state == "completed"
    assert result.verification_run.verdict_state == "fail"
    assert result.verification_run.recovery_state is not None
    assert result.verification_run.recovery_state.runtime_state == "reporting"
    assert result.verification_run.recovery_state.recovery_reason is None
    assert result.verification_run.verification_confidence == "degraded"
    assert result.evidence_set.summary.late_count == 1
    assert result.verification_run.verification_steps[0].evidence_status == "late"
    assert result.verification_run.verification_steps[0].verdict_state == "fail"
    assert repo.rows[0]["evidence_status"] == "late"


@pytest.mark.anyio
async def test_execute_simulated_verification_run_splits_multi_ied_sessions_and_keeps_group_context() -> None:
    plan = _build_multi_ied_plan()
    repo = _FakeVerificationEvidenceRepository()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)

    result = await execute_simulated_verification_run(
        workspace_id=7,
        test_run_id="run-3",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
            selected_group_id="group-42",
            operator_id="operator-7",
        ),
        repository=repo,  # type: ignore[arg-type]
        triggered_at=triggered_at,
        latency_ms=250,
        now=lambda: triggered_at + timedelta(milliseconds=250),
    )

    assert result.verification_run.selected_group_id == "group-42"
    assert result.verification_run.operator_id == "operator-7"
    assert len(result.verification_run.session_snapshots) == 2
    assert len(result.verification_run.subscription_snapshots) == 2
    assert {snapshot.endpoint_id for snapshot in result.verification_run.session_snapshots} == {
        "sim:IED-A/P1",
        "sim:IED-B/P1",
    }
    assert {snapshot.subscription_state for snapshot in result.verification_run.subscription_snapshots} == {
        "reporting",
    }
    assert result.verification_run.verdict_state == "pass"
    assert result.evidence_set.summary.evidence_count == 2
    assert result.evidence_set.summary.source_generation == 1
    assert {row["endpoint_id"] for row in repo.rows} == {"sim:IED-A/P1", "sim:IED-B/P1"}
    assert {row["source_generation"] for row in repo.rows} == {1}
    assert {step.source_report_rpt_id for step in result.verification_run.verification_steps} == {
        "IED-A/LLN0.brA",
        "IED-B/LLN0.brB",
    }
    assert {step.group_id for step in result.verification_run.verification_steps} == {"group-1", "group-2"}
    assert {step.verification_confidence for step in result.verification_run.verification_steps} == {"exact_report_match"}
    assert {
        step.source_session_id for step in result.verification_run.verification_steps
    } == {"run-3:sim:IED-A/P1", "run-3:sim:IED-B/P1"}
    assert result.verification_run.recovery_state is not None
    assert result.verification_run.recovery_state.runtime_state == "reporting"
    assert result.verification_run.recovery_state.recovery_reason is None


@pytest.mark.anyio
async def test_execute_simulated_verification_run_reuses_one_session_for_same_ied_multi_signal_selection() -> None:
    plan = _build_same_ied_plan()
    repo = _FakeVerificationEvidenceRepository()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)

    result = await execute_simulated_verification_run(
        workspace_id=7,
        test_run_id="run-same-ied",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        repository=repo,  # type: ignore[arg-type]
        triggered_at=triggered_at,
        latency_ms=250,
        now=lambda: triggered_at + timedelta(milliseconds=250),
    )

    assert result.verification_run.verdict_state == "pass"
    assert len(result.verification_run.verification_steps) == 2
    assert len(result.verification_run.session_snapshots) == 1
    assert len(result.verification_run.subscription_snapshots) == 1
    assert result.verification_run.session_snapshots[0].endpoint_id == "sim:IED-A/P1"
    assert result.verification_run.subscription_snapshots[0].endpoint_id == "sim:IED-A/P1"
    assert result.verification_run.verification_confidence == "exact_report_match"
    assert {step.group_id for step in result.verification_run.verification_steps} == {"group-1"}
    assert {step.source_session_id for step in result.verification_run.verification_steps} == {
        "run-same-ied:sim:IED-A/P1"
    }


@pytest.mark.anyio
async def test_execute_simulated_verification_run_marks_missing_observation_as_timeout_and_degraded() -> None:
    plan = _build_plan()
    repo = _FakeVerificationEvidenceRepository()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)

    result = await execute_simulated_verification_run(
        workspace_id=7,
        test_run_id="run-4",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        repository=repo,  # type: ignore[arg-type]
        triggered_at=triggered_at,
        latency_ms=250,
        now=lambda: triggered_at + timedelta(milliseconds=250),
        simulate_missing_signal_ids=(101,),
    )

    assert result.verification_run.verdict_state == "fail"
    assert result.verification_run.recovery_state is not None
    assert result.verification_run.recovery_state.runtime_state == "degraded"
    assert result.verification_run.recovery_state.desired_state == "reconnecting"
    assert result.verification_run.recovery_state.recovery_reason == "timeout"
    assert result.verification_run.verification_confidence == "degraded"
    assert result.verification_run.recovery_state.stale_signal_count == 1
    assert result.verification_run.recovery_state.preserved_evidence_count == 1
    assert result.verification_run.recovery_state.desired_target_ids == [101]
    assert result.verification_run.verification_steps[0].evidence_status == "timeout"
    assert result.verification_run.verification_steps[0].verdict_state == "fail"
    assert result.evidence_set.summary.timeout_count == 1
    assert result.evidence_set.summary.source_generation == 1
    assert repo.rows[0]["evidence_status"] == "timeout"


@pytest.mark.anyio
async def test_execute_simulated_verification_run_marks_stale_generation_as_degraded() -> None:
    plan = _build_plan()
    repo = _FakeVerificationEvidenceRepository()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)

    result = await execute_simulated_verification_run(
        workspace_id=7,
        test_run_id="run-5",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        repository=repo,  # type: ignore[arg-type]
        triggered_at=triggered_at,
        latency_ms=250,
        now=lambda: triggered_at + timedelta(milliseconds=250),
        simulate_stale_signal_ids=(101,),
    )

    assert result.verification_run.verdict_state == "fail"
    assert result.verification_run.recovery_state is not None
    assert result.verification_run.recovery_state.runtime_state == "degraded"
    assert result.verification_run.recovery_state.desired_state == "reconnecting"
    assert result.verification_run.recovery_state.recovery_reason == "stale_generation"
    assert result.verification_run.verification_confidence == "degraded"
    assert result.verification_run.recovery_state.stale_signal_count == 1
    assert result.verification_run.verification_steps[0].evidence_status == "stale"
    assert result.verification_run.verification_steps[0].verdict_state == "fail"
    assert result.evidence_set.summary.stale_count == 1
    assert result.evidence_set.summary.source_generation == 1
    assert repo.rows[0]["evidence_status"] == "stale"
