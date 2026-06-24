from __future__ import annotations

from types import SimpleNamespace

from app.services.verification_planner import (
    build_planner_confidence_report,
    VerificationTargetSource,
    build_verification_subscription_plan,
    build_verification_target_sources,
)


def test_build_verification_subscription_plan_separates_exact_partial_and_uncovered_targets() -> None:
    plan = build_verification_subscription_plan(
        [
            VerificationTargetSource(
                signal_id=1,
                signal_reference="Pump Start",
                signal_path="pump_start",
                signal_metadata={
                    "protocol": "iec61850",
                    "row_index": 17,
                    "protocol_metadata": {
                        "expected_feedback_path": "KINTE13LVC01CTRL/LLN0.RCB1",
                        "data_reference": "PROT/A8750PDIF1$ST$Op$general",
                    },
                },
                source_row_index=17,
                source_kind=None,
                source_reason=None,
                allocation_id=101,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=7,
                channel_label="DO-7",
                unit_id="unit-a",
                unit_online=True,
                source_row_id="signal-1",
            ),
            VerificationTargetSource(
                signal_id=2,
                signal_reference="Pump Feedback",
                signal_path="pump_feedback",
                signal_metadata={},
                source_row_index=None,
                source_kind=None,
                source_reason=None,
                allocation_id=102,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": True,
                    "stale_device": False,
                },
                channel_id=8,
                channel_label="DO-8",
                unit_id="unit-b",
                unit_online=False,
                source_row_id="signal-2",
            ),
            VerificationTargetSource(
                signal_id=3,
                signal_reference="Unassigned",
                signal_path="unassigned",
                signal_metadata={},
                source_row_index=None,
                source_kind=None,
                source_reason=None,
                allocation_id=None,
                allocation_status="unassigned",
                allocation_health={},
                channel_id=None,
                channel_label=None,
                unit_id=None,
                unit_online=None,
                source_row_id="signal-3",
            ),
        ]
    )

    assert [target.signal_id for target in plan.targets] == [1, 2, 3]
    assert plan.targets[0].coverage_state == "exact"
    assert plan.targets[0].endpoint_id == "sim:unit-a/unknown"
    assert plan.targets[0].expected_feedback_path == "KINTE13LVC01CTRL/LLN0.RCB1"
    assert plan.targets[0].protocol == "iec61850"
    assert plan.targets[0].source_row_index == 17
    assert plan.targets[1].coverage_state == "partial"
    assert plan.targets[1].endpoint_id == "sim:unit-b/unknown"
    assert plan.targets[1].coverage_reason == "allocation_offline_device"
    assert plan.targets[1].expected_feedback_path == "pump_feedback"
    assert plan.targets[2].coverage_state == "uncovered"
    assert plan.targets[2].endpoint_id is None
    assert plan.targets[2].coverage_reason == "no_endpoint"
    assert plan.plan_id.startswith("plan-")
    assert len(plan.groups) == 2
    assert [group.target_indexes for group in plan.groups] == [[0], [1]]
    assert [group.source_classification for group in plan.groups] == ["fallback", "fallback"]
    assert plan.groups[0].endpoint_id == "unit-a"
    assert plan.groups[1].endpoint_id == "unit-b"
    assert plan.groups[0].report_control_reference == "KINTE13LVC01CTRL/LLN0.RCB1"
    assert plan.groups[0].report_control_name == "RCB1"
    assert plan.groups[1].report_control_reference == "pump_feedback"
    assert len(plan.uncovered_targets) == 1
    assert plan.uncovered_targets[0].target_index == 2
    assert plan.uncovered_targets[0].reason == "no_endpoint"
    assert plan.uncovered_targets[0].detail == "no_endpoint"
    assert "normalized 3 verification targets" in plan.planning_diagnostics[0]
    assert "2 subscription groups across 2 endpoints" in plan.planning_diagnostics[1]

    assert plan.coverage.total_targets == 3
    assert plan.coverage.covered_targets == 1
    assert plan.coverage.partially_covered_targets == 1
    assert plan.coverage.uncovered_targets == 1
    assert plan.coverage.groups_count == 2
    assert plan.coverage.endpoints_count == 2
    assert plan.coverage.planning_quality == "partial"


def test_build_verification_subscription_plan_groups_targets_by_scd_hints() -> None:
    plan = build_verification_subscription_plan(
        [
            VerificationTargetSource(
                signal_id=10,
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
                        "rpt_id": "IED-ALD0/LLN0.brA",
                        "data_set_reference": "IED-ALD0/LLN0.dsA",
                        "expected_feedback_path": "IED-ALD0/LLN0.brA",
                    },
                },
                allocation_id=301,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=41,
                channel_label="DO-41",
                unit_id="IED-A/P1",
                unit_online=True,
                source_row_id="signal-10",
            ),
            VerificationTargetSource(
                signal_id=11,
                signal_reference="Breaker Feedback",
                signal_path="breaker_feedback",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "ied_name": "IED-A",
                        "access_point_name": "P1",
                        "report_control_reference_hint": "IED-A/P1/LLN0.brA/buffered",
                        "report_control_name": "brA",
                        "report_kind": "buffered",
                        "rpt_id": "IED-ALD0/LLN0.brA",
                        "data_set_reference": "IED-ALD0/LLN0.dsA",
                        "expected_feedback_path": "IED-ALD0/LLN0.brA",
                    },
                },
                allocation_id=302,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=42,
                channel_label="DI-42",
                unit_id="IED-A/P1",
                unit_online=True,
                source_row_id="signal-11",
            ),
            VerificationTargetSource(
                signal_id=12,
                signal_reference="Uncovered",
                signal_path="uncovered",
                signal_metadata={},
                allocation_id=None,
                allocation_status="unassigned",
                allocation_health={},
                channel_id=None,
                channel_label=None,
                unit_id=None,
                unit_online=None,
                source_row_id="signal-12",
            ),
        ]
    )

    assert plan.plan_id.startswith("plan-")
    assert [target.signal_id for target in plan.targets] == [10, 11, 12]
    assert plan.targets[0].endpoint_id == "sim:IED-A/P1/unknown"
    assert len(plan.groups) == 1
    assert plan.groups[0].group_id == "group-1"
    assert plan.groups[0].source_classification == "from SCD"
    assert plan.groups[0].endpoint_id == "IED-A/P1"
    assert plan.groups[0].ied_name == "IED-A"
    assert plan.groups[0].access_point_name == "P1"
    assert plan.groups[0].report_control_reference == "IED-A/P1/LLN0.brA/buffered"
    assert plan.groups[0].report_control_name == "brA"
    assert plan.groups[0].report_kind == "buffered"
    assert plan.groups[0].rpt_id == "IED-ALD0/LLN0.brA"
    assert plan.groups[0].data_set_reference == "IED-ALD0/LLN0.dsA"
    assert plan.groups[0].target_indexes == [0, 1]
    assert plan.groups[0].reason == "SCD hint match"
    assert len(plan.uncovered_targets) == 1
    assert plan.uncovered_targets[0].target_index == 2
    assert plan.uncovered_targets[0].reason == "no_endpoint"
    assert plan.coverage.total_targets == 3
    assert plan.coverage.covered_targets == 2
    assert plan.coverage.partially_covered_targets == 0
    assert plan.coverage.uncovered_targets == 1
    assert plan.coverage.groups_count == 1
    assert plan.coverage.endpoints_count == 1
    assert plan.coverage.planning_quality == "partial"


def test_build_verification_target_sources_preserves_signal_identity_and_row_metadata() -> None:
    signals_by_id = {
        11: SimpleNamespace(
            id=11,
            name="Pump Start",
            key="pump_start",
            signal_metadata={
                "row_index": 4,
                "source_kind": "discovery",
                "source_reason": "derived from discovered dataset membership",
                "protocol_metadata": {
                    "expected_feedback_path": "IED1LD0/LLN0.RCB1",
                },
            },
        ),
        12: SimpleNamespace(
            id=12,
            name="Auxiliary",
            key="aux_signal",
            signal_metadata={},
        ),
    }
    allocation_rows_by_signal_id = {
        11: SimpleNamespace(
            row_id="signal-11",
            allocation_id=201,
            allocation_status="assigned",
            allocation_health={"offline_device": False},
            channel_id=31,
            channel_label="AP1/DO-31",
            unit_id="ied-1",
            unit_online=True,
        ),
        12: SimpleNamespace(
            row_id="signal-12",
            allocation_id=None,
            allocation_status="unassigned",
            allocation_health={},
            channel_id=None,
            channel_label=None,
            unit_id=None,
            unit_online=None,
        ),
    }

    sources = build_verification_target_sources(
        requested_signal_ids=[11, 11, 12, 0, -4],
        signals_by_id=signals_by_id,
        allocation_rows_by_signal_id=allocation_rows_by_signal_id,
    )

    assert [source.signal_id for source in sources] == [11, 12]
    assert sources[0].signal_reference == "Pump Start"
    assert sources[0].signal_path == "pump_start"
    assert sources[0].source_row_index == 4
    assert sources[0].source_kind == "discovery"
    assert sources[0].source_reason == "derived from discovered dataset membership"
    assert sources[0].source_row_id == "signal-11"
    assert sources[0].unit_id == "ied-1"
    assert sources[0].allocation_id == 201
    assert sources[1].signal_reference == "Auxiliary"
    assert sources[1].source_row_index is None
    assert sources[1].source_kind is None
    assert sources[1].source_reason is None
    assert sources[1].source_row_id == "signal-12"


def test_build_planner_confidence_report_summarizes_coverage_and_origin_labels() -> None:
    plan = build_verification_subscription_plan(
        [
            VerificationTargetSource(
                signal_id=1,
                signal_reference="SCD Signal",
                signal_path="scd_signal",
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
                source_row_id="signal-1",
                source_kind="from SCD",
                source_reason="SCD hint match",
            ),
            VerificationTargetSource(
                signal_id=2,
                signal_reference="Discovery Signal",
                signal_path="discovery_signal",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "ied_name": "IED-B",
                        "access_point_name": "P1",
                        "report_control_name": "brB",
                        "report_kind": "buffered",
                        "rpt_id": "IED-B/LLN0.brB",
                        "data_set_reference": "IED-B/LLN0.dsB",
                        "expected_feedback_path": "LD0/XCBR2.Pos.stVal[ST]",
                        "source_kind": "discovery",
                        "source_reason": "derived from discovered dataset membership",
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
                unit_id="IED-B/P1",
                unit_online=True,
                source_row_id="signal-2",
            ),
            VerificationTargetSource(
                signal_id=3,
                signal_reference="Fallback Signal",
                signal_path="fallback_signal",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "expected_feedback_path": "LD0/XCBR3.Pos.stVal[ST]",
                    },
                },
                allocation_id=3,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=13,
                channel_label="DO-13",
                unit_id="IED-C/P1",
                unit_online=True,
                source_row_id="signal-3",
            ),
            VerificationTargetSource(
                signal_id=4,
                signal_reference="Uncovered Signal",
                signal_path="uncovered_signal",
                signal_metadata={},
                allocation_id=None,
                allocation_status="unassigned",
                allocation_health={},
                channel_id=None,
                channel_label=None,
                unit_id=None,
                unit_online=None,
                source_row_id="signal-4",
            ),
        ]
    )

    report = build_planner_confidence_report(plan)

    assert report.plan_id == plan.plan_id
    assert report.total_targets == 4
    assert report.covered_targets == 3
    assert report.partially_covered_targets == 0
    assert report.uncovered_targets == 1
    assert report.coverage_percentage == 75
    assert report.confidence_percentage == 51
    assert report.risk_level == "high"
    assert report.source_classification_counts == {
        "from SCD": 1,
        "from discovery": 1,
        "fallback": 1,
        "not found": 1,
    }
    assert [signal.source_classification for signal in report.signals] == [
        "from SCD",
        "from discovery",
        "fallback",
        "not found",
    ]
    assert [signal.confidence_state for signal in report.signals] == [
        "strong",
        "watch",
        "risk",
        "uncovered",
    ]
    assert report.signals[3].coverage_state == "uncovered"
    assert any("Fallback Signal" in diagnostic for diagnostic in report.diagnostics)
    assert any("require attention" in diagnostic for diagnostic in report.diagnostics)
