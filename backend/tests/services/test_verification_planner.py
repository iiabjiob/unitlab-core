from __future__ import annotations

from app.services.verification_planner import (
    VerificationTargetSource,
    build_verification_subscription_plan,
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
                    "protocol_metadata": {
                        "expected_feedback_path": "KINTE13LVC01CTRL/LLN0.RCB1",
                        "data_reference": "PROT/A8750PDIF1$ST$Op$general",
                    },
                },
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
    assert plan.targets[0].expected_feedback_path == "KINTE13LVC01CTRL/LLN0.RCB1"
    assert plan.targets[0].protocol == "iec61850"
    assert plan.targets[1].coverage_state == "partial"
    assert plan.targets[1].coverage_reason == "allocation_offline_device"
    assert plan.targets[1].expected_feedback_path == "pump_feedback"
    assert plan.targets[2].coverage_state == "uncovered"
    assert plan.targets[2].coverage_reason == "no_endpoint"

    assert plan.coverage.total_targets == 3
    assert plan.coverage.covered_targets == 1
    assert plan.coverage.partially_covered_targets == 1
    assert plan.coverage.uncovered_targets == 1
    assert plan.coverage.groups_count == 3
    assert plan.coverage.endpoints_count == 2
    assert plan.coverage.planning_quality == "partial"
