from __future__ import annotations

from datetime import UTC, datetime, timedelta
from pathlib import Path
import json

import pytest

from app.schemas.verification_schema import VerificationExecutionContextSchema
from app.services.iec61850.report_runtime import Iec61850DeviceEndpoint, Iec61850RuntimeMode
from app.services.verification_planner import VerificationTargetSource, build_verification_subscription_plan
from app.services.verification_regression_harness import (
    VerificationRegressionCase,
    VerificationRegressionExpectation,
    run_verification_regression_case,
    run_verification_regression_suite,
    write_verification_regression_artifacts,
)


def _build_exact_plan():
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
            ),
        ]
    )


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


def _virtual_endpoint_for_device(device) -> Iec61850DeviceEndpoint:
    return Iec61850DeviceEndpoint(
        id=f"sim:{device.ied_name}/{device.access_point_name}@10.10.10.250:12447",
        mode=Iec61850RuntimeMode.SIMULATOR,
        ied_name=device.ied_name,
        access_point_name=device.access_point_name,
        host="10.10.10.250",
        port=12447,
    )


@pytest.mark.anyio
async def test_verification_regression_case_captures_pass_fixture_and_report() -> None:
    plan = _build_exact_plan()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)
    case = VerificationRegressionCase(
        scenario_id="reg-pass",
        mode="verification_run",
        description="single-signal pass",
        verification_targets=tuple(plan.targets),
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        expectation=VerificationRegressionExpectation(
            verdict_state="pass",
            verification_confidence="exact_report_match",
            confidence_reason="exact_dataset_match",
            runtime_state="reporting",
            evidence_count=1,
            session_count=1,
            subscription_count=1,
            step_count=1,
        ),
    )

    result = await run_verification_regression_case(
        case,
        triggered_at=triggered_at,
        now=lambda: triggered_at + timedelta(milliseconds=250),
    )

    assert result.passed is True
    assert result.fixture_payload["schema"] == "unitlab.iec61850.ied-simulator-fixture.v1"
    assert result.report_payload["passed"] is True
    assert result.report_payload["verification_run"]["verdict_state"] == "pass"
    assert result.report_payload["verification_run"]["verification_confidence"] == "exact_report_match"
    assert result.report_payload["verification_run"]["runtime_state"] == "reporting"
    assert result.report_payload["evidence_set"]["summary"]["evidence_count"] == 1


@pytest.mark.anyio
async def test_verification_regression_case_uses_custom_endpoint_mapper() -> None:
    plan = _build_exact_plan()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)
    case = VerificationRegressionCase(
        scenario_id="reg-custom-endpoint",
        mode="verification_run",
        description="single-signal run on virtual endpoint",
        verification_targets=tuple(plan.targets),
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        expectation=VerificationRegressionExpectation(
            verdict_state="pass",
            verification_confidence="exact_report_match",
            confidence_reason="exact_dataset_match",
            runtime_state="reporting",
            evidence_count=1,
            session_count=1,
            subscription_count=1,
            step_count=1,
        ),
        endpoint_for_device=_virtual_endpoint_for_device,
    )

    result = await run_verification_regression_case(
        case,
        triggered_at=triggered_at,
        now=lambda: triggered_at + timedelta(milliseconds=250),
    )

    assert result.passed is True
    assert result.verification_run is not None
    assert result.verification_run.session_snapshots[0].endpoint_id == "sim:IED-A/P1@10.10.10.250:12447"
    assert result.verification_run.subscription_snapshots[0].endpoint_id == "sim:IED-A/P1@10.10.10.250:12447"


@pytest.mark.anyio
async def test_verification_regression_case_captures_timeout_and_recovery() -> None:
    plan = _build_exact_plan()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)
    case = VerificationRegressionCase(
        scenario_id="reg-timeout",
        mode="verification_run",
        description="missing signal times out",
        verification_targets=tuple(plan.targets),
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        expectation=VerificationRegressionExpectation(
            verdict_state="fail",
            verification_confidence="degraded",
            recovery_reason="timeout",
            runtime_state="reporting",
            recovery_runtime_state="degraded",
            evidence_count=1,
            session_count=1,
            subscription_count=1,
            step_count=1,
        ),
        simulate_missing_signal_ids=(101,),
    )

    result = await run_verification_regression_case(
        case,
        triggered_at=triggered_at,
        now=lambda: triggered_at + timedelta(milliseconds=250),
    )

    assert result.passed is True
    assert result.report_payload["passed"] is True
    assert result.report_payload["verification_run"]["verdict_state"] == "fail"
    assert result.report_payload["verification_run"]["recovery_state"]["recovery_reason"] == "timeout"
    assert result.report_payload["verification_run"]["runtime_state"] == "reporting"
    assert result.report_payload["verification_run"]["verification_confidence"] == "degraded"


@pytest.mark.anyio
async def test_verification_regression_case_captures_reconnect_generation_bump() -> None:
    plan = _build_multi_ied_plan()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)
    case = VerificationRegressionCase(
        scenario_id="reg-reconnect",
        mode="reconnect",
        description="reconnect one session in multi-ied run",
        verification_targets=tuple(plan.targets),
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        expectation=VerificationRegressionExpectation(
            runtime_state="reporting",
            session_count=2,
            subscription_count=2,
            active_generation=2,
        ),
    )

    result = await run_verification_regression_case(
        case,
        triggered_at=triggered_at,
        now=lambda: triggered_at + timedelta(milliseconds=250),
    )

    assert result.passed is True
    assert result.report_payload["passed"] is True
    assert result.report_payload["verification_run"]["runtime_state"] == "reporting"
    assert max(snapshot["connection_generation"] for snapshot in result.report_payload["session_snapshots"]) == 2
    assert result.report_payload["subscription_snapshots"][0]["subscription_state"] == "reporting"


@pytest.mark.anyio
async def test_verification_regression_suite_writes_artifacts(tmp_path: Path) -> None:
    pass_plan = _build_exact_plan()
    multi_plan = _build_multi_ied_plan()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)
    cases = [
        VerificationRegressionCase(
            scenario_id="reg-pass-artifacts",
            mode="verification_run",
            description="single-signal pass",
            verification_targets=tuple(pass_plan.targets),
            subscription_plan=pass_plan,
            execution_context=VerificationExecutionContextSchema(
                project_id=1,
                signal_list_revision_id=2,
                planner_version="test",
                runtime_version="simulator",
                policy_version="v1",
            ),
            expectation=VerificationRegressionExpectation(
                verdict_state="pass",
                verification_confidence="exact_report_match",
                confidence_reason="exact_dataset_match",
                runtime_state="reporting",
                evidence_count=1,
                session_count=1,
                subscription_count=1,
                step_count=1,
            ),
        ),
        VerificationRegressionCase(
            scenario_id="reg-reconnect-artifacts",
            mode="reconnect",
            description="reconnect one session in multi-ied run",
            verification_targets=tuple(multi_plan.targets),
            subscription_plan=multi_plan,
            execution_context=VerificationExecutionContextSchema(
                project_id=1,
                signal_list_revision_id=2,
                planner_version="test",
                runtime_version="simulator",
                policy_version="v1",
            ),
            expectation=VerificationRegressionExpectation(
                runtime_state="reporting",
                session_count=2,
                subscription_count=2,
                active_generation=2,
            ),
        ),
    ]

    suite = await run_verification_regression_suite(
        cases,
        triggered_at=triggered_at,
        now=lambda: triggered_at + timedelta(milliseconds=250),
    )

    artifact_root = tmp_path / "artifacts"
    manifest = write_verification_regression_artifacts(suite, artifact_root)

    assert manifest["schema"] == "unitlab.verification.regression-artifacts.v1"
    assert manifest["suite_id"] == suite.suite_id
    assert manifest["suite_report_path"] == "suite-report.json"
    assert len(manifest["cases"]) == 2
    assert (artifact_root / "manifest.json").exists()
    assert (artifact_root / "suite-report.json").exists()
    assert (artifact_root / "cases" / "reg-pass-artifacts" / "report.json").exists()
    assert (artifact_root / "cases" / "reg-pass-artifacts" / "fixture.json").exists()

    manifest_data = json.loads((artifact_root / "manifest.json").read_text(encoding="utf-8"))
    suite_report_data = json.loads((artifact_root / "suite-report.json").read_text(encoding="utf-8"))
    case_report_data = json.loads((artifact_root / "cases" / "reg-pass-artifacts" / "report.json").read_text(encoding="utf-8"))
    assert manifest_data["cases"][0]["scenario_id"] == "reg-pass-artifacts"
    assert suite_report_data["suite_id"] == suite.suite_id
    assert case_report_data["scenario_id"] == "reg-pass-artifacts"
    assert case_report_data["verification_run"]["verdict_state"] == "pass"


@pytest.mark.anyio
async def test_verification_regression_suite_aggregates_results_into_report() -> None:
    pass_plan = _build_exact_plan()
    multi_plan = _build_multi_ied_plan()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)
    cases = [
        VerificationRegressionCase(
            scenario_id="reg-pass-suite",
            mode="verification_run",
            description="single-signal pass",
            verification_targets=tuple(pass_plan.targets),
            subscription_plan=pass_plan,
            execution_context=VerificationExecutionContextSchema(
                project_id=1,
                signal_list_revision_id=2,
                planner_version="test",
                runtime_version="simulator",
                policy_version="v1",
            ),
            expectation=VerificationRegressionExpectation(
                verdict_state="pass",
                verification_confidence="exact_report_match",
                confidence_reason="exact_dataset_match",
                runtime_state="reporting",
                evidence_count=1,
                session_count=1,
                subscription_count=1,
                step_count=1,
            ),
        ),
        VerificationRegressionCase(
            scenario_id="reg-timeout-suite",
            mode="verification_run",
            description="missing signal times out",
            verification_targets=tuple(pass_plan.targets),
            subscription_plan=pass_plan,
            execution_context=VerificationExecutionContextSchema(
                project_id=1,
                signal_list_revision_id=2,
                planner_version="test",
                runtime_version="simulator",
                policy_version="v1",
            ),
        expectation=VerificationRegressionExpectation(
            verdict_state="fail",
            verification_confidence="degraded",
            recovery_reason="timeout",
            runtime_state="reporting",
            recovery_runtime_state="degraded",
            evidence_count=1,
            session_count=1,
            subscription_count=1,
            step_count=1,
            ),
            simulate_missing_signal_ids=(101,),
        ),
        VerificationRegressionCase(
            scenario_id="reg-reconnect-suite",
            mode="reconnect",
            description="reconnect one session in multi-ied run",
            verification_targets=tuple(multi_plan.targets),
            subscription_plan=multi_plan,
            execution_context=VerificationExecutionContextSchema(
                project_id=1,
                signal_list_revision_id=2,
                planner_version="test",
                runtime_version="simulator",
                policy_version="v1",
            ),
            expectation=VerificationRegressionExpectation(
                runtime_state="reporting",
                session_count=2,
                subscription_count=2,
                active_generation=2,
            ),
        ),
    ]

    suite = await run_verification_regression_suite(
        cases,
        triggered_at=triggered_at,
        now=lambda: triggered_at + timedelta(milliseconds=250),
    )

    assert suite.passed is True
    assert suite.summary["case_count"] == 3
    assert suite.summary["passed_count"] == 3
    assert suite.summary["fixture_schema"] == "unitlab.iec61850.ied-simulator-fixture.v1"
    assert len(suite.results) == 3
    assert all(result.passed for result in suite.results)
    assert suite.to_report()["results"][0]["verification_run"]["verdict_state"] in {"pass", "fail"}
