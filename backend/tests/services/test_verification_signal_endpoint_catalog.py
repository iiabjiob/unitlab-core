from __future__ import annotations

from types import SimpleNamespace

from app.schemas.verification_schema import (
    VerificationTargetSchema,
    VerificationSubscriptionPlanCoverageSchema,
    VerificationSubscriptionPlanGroupSchema,
    VerificationSubscriptionPlanSchema,
)
from app.services.verification_planner import VerificationTargetSource
from app.services.verification_planner import build_verification_subscription_plan
from app.services.verification_signal_endpoint_catalog import (
    build_verification_plan_endpoint_catalog,
    build_verification_signal_endpoint_catalog,
)


def test_build_verification_signal_endpoint_catalog_uses_signal_metadata_host() -> None:
    sources = [
        VerificationTargetSource(
            signal_id=101,
            signal_reference="Breaker Close",
            signal_path="breaker_close",
            signal_metadata={
                "protocol": "iec61850",
                "protocol_metadata": {
                    "ied_name": "IED-A",
                    "access_point_name": "P1",
                    "transport_host": "10.10.10.250",
                    "report_control_name": "brA",
                    "report_kind": "buffered",
                    "rpt_id": "IED-A/LLN0.brA",
                    "data_set_reference": "IED-A/LLN0.dsA",
                },
            },
            allocation_id=1,
            allocation_status="assigned",
            allocation_health={},
            channel_id=11,
            channel_label="DO-11",
            unit_id="IED-A/P1",
            unit_online=True,
            source_row_id="signal-101",
        )
    ]
    plan = VerificationSubscriptionPlanSchema(
        plan_id="plan-1",
        selected_signal_ids=[101],
        targets=[],
        groups=[
            VerificationSubscriptionPlanGroupSchema(
                group_id="group-1",
                endpoint_id="IED-A/P1",
                ied_name="IED-A",
                access_point_name="P1",
                report_control_reference="IED-A/P1/LLN0.brA/buffered",
                report_control_name="brA",
                report_kind="buffered",
                rpt_id="IED-A/LLN0.brA",
                data_set_reference="IED-A/LLN0.dsA",
                target_indexes=[0],
                reason="fallback endpoint binding",
                source_classification="fallback",
                source_reason="allocation_offline_device",
            )
        ],
        uncovered_targets=[],
        planning_diagnostics=[],
        coverage=VerificationSubscriptionPlanCoverageSchema(
            total_targets=1,
            covered_targets=0,
            partially_covered_targets=1,
            uncovered_targets=0,
            groups_count=1,
            endpoints_count=1,
            planning_quality="fallback",
        ),
    )

    catalog = build_verification_signal_endpoint_catalog(sources=sources, subscription_plan=plan)

    assert catalog is not None
    endpoint, notes = catalog.resolve_transport_endpoint(
        ied_name="IED-A",
        access_point_name="P1",
        requested_host=None,
        requested_port=None,
    )
    assert endpoint.id == "mms:IED-A/P1@10.10.10.250:102"
    assert endpoint.host == "10.10.10.250"
    assert endpoint.port == 102
    assert notes == ("transport host resolved from endpoint catalog",)


def test_signal_list_only_metadata_builds_mms_endpoint_catalog_with_port() -> None:
    sources = [
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
            unit_id="KINTE15BCU01/AP1",
            unit_online=None,
            source_row_id="signal-101",
        )
    ]
    plan = build_verification_subscription_plan(sources)

    assert plan.targets[0].protocol == "iec61850"
    assert plan.targets[0].expected_feedback_path == "KINTE15BCU01CTRL1/CBCSWI1/Pos/stVal[ST]"
    assert plan.targets[0].protocol_metadata["transport_host"] == "172.16.40.128:12447"
    assert "ied_name" not in plan.targets[0].protocol_metadata
    assert plan.targets[0].protocol_metadata["access_point_name"] == "AP1"
    assert plan.targets[0].coverage_state == "partial"
    assert plan.groups[0].ied_name is None
    assert plan.groups[0].endpoint_id == "172.16.40.128:12447"
    assert plan.groups[0].access_point_name == "AP1"

    catalog = build_verification_signal_endpoint_catalog(sources=sources, subscription_plan=plan)

    assert catalog is not None
    endpoint = catalog.endpoint_for_plan_device(
        SimpleNamespace(
            ied_name="",
            access_point_name="AP1",
            endpoint_id="172.16.40.128:12447",
        )
    )
    assert endpoint.id == "172.16.40.128:12447"
    assert endpoint.ied_name == ""
    assert endpoint.host == "172.16.40.128"
    assert endpoint.port == 12447


def test_signal_list_address_domain_is_not_treated_as_ied_name() -> None:
    source = VerificationTargetSource(
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

    plan = build_verification_subscription_plan([source])

    assert "ied_name" not in plan.targets[0].protocol_metadata
    assert plan.groups[0].ied_name is None
    assert plan.groups[0].endpoint_id == "172.16.40.128:12447"

    catalog = build_verification_signal_endpoint_catalog(sources=[source], subscription_plan=plan)

    assert catalog is not None
    endpoint = catalog.endpoint_for_plan_device(
        SimpleNamespace(
            ied_name="",
            access_point_name="AP1",
            endpoint_id="172.16.40.128:12447",
        )
    )
    assert endpoint.id == "172.16.40.128:12447"
    assert endpoint.ied_name == ""
    assert endpoint.access_point_name == "AP1"
    assert endpoint.host == "172.16.40.128"
    assert endpoint.port == 12447


def test_signal_list_only_groups_signal_list_addresses_by_transport_endpoint_and_logical_node() -> None:
    sources = [
        VerificationTargetSource(
            signal_id=101,
            signal_reference="Switch position",
            signal_path="switch_position",
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
            unit_id="KINTE15BCU01/AP1",
            unit_online=None,
            source_row_id="signal-101",
        ),
        VerificationTargetSource(
            signal_id=102,
            signal_reference="Switch quality",
            signal_path="switch_quality",
            signal_metadata={
                "row": {
                    "transport_host": "172.16.40.128:12447",
                    "iec61850_address": "KINTE15BCU01CTRL1/CBCSWI1/Pos/q[ST]",
                },
                "verification": {
                    "enabled": True,
                    "transport_host": "172.16.40.128:12447",
                    "iec61850_address": "KINTE15BCU01CTRL1/CBCSWI1/Pos/q[ST]",
                },
            },
            allocation_id=None,
            allocation_status="unassigned",
            allocation_health={},
            channel_id=None,
            channel_label=None,
            unit_id="KINTE15BCU01/AP1",
            unit_online=None,
            source_row_id="signal-102",
        ),
        VerificationTargetSource(
            signal_id=103,
            signal_reference="Indication",
            signal_path="indication",
            signal_metadata={
                "row": {
                    "transport_host": "172.16.40.128:12447",
                    "iec61850_address": "KINTE15BCU01CTRL2/GAPC1/Ind1/stVal[ST]",
                },
                "verification": {
                    "enabled": True,
                    "transport_host": "172.16.40.128:12447",
                    "iec61850_address": "KINTE15BCU01CTRL2/GAPC1/Ind1/stVal[ST]",
                },
            },
            allocation_id=None,
            allocation_status="unassigned",
            allocation_health={},
            channel_id=None,
            channel_label=None,
            unit_id="KINTE15BCU01CTRL2/AP1",
            unit_online=None,
            source_row_id="signal-103",
        ),
    ]

    plan = build_verification_subscription_plan(sources)

    assert len(plan.groups) == 2
    assert {group.endpoint_id for group in plan.groups} == {"172.16.40.128:12447"}
    assert {group.ied_name for group in plan.groups} == {None}
    assert sorted(group.target_indexes for group in plan.groups) == [[0, 1], [2]]


def test_build_verification_plan_endpoint_catalog_uses_target_protocol_metadata_host_port() -> None:
    plan = VerificationSubscriptionPlanSchema(
        plan_id="plan-1",
        selected_signal_ids=[101],
        targets=[
            VerificationTargetSchema(
                signal_id=101,
                signal_reference="Breaker Close",
                signal_path="breaker_close",
                endpoint_id="IED-A/P1",
                expected_feedback_path="LD0/XCBR1.Pos.stVal[ST]",
                protocol="iec61850",
                protocol_metadata={
                    "ied_name": "IED-A",
                    "access_point_name": "P1",
                    "transport_host": "10.10.10.250:12447",
                },
                coverage_state="partial",
                unit_id="IED-A/P1",
            )
        ],
        groups=[
            VerificationSubscriptionPlanGroupSchema(
                group_id="group-1",
                endpoint_id="IED-A/P1",
                ied_name="IED-A",
                access_point_name="P1",
                report_control_reference="IED-A/P1/LLN0.brA/buffered",
                report_control_name="brA",
                report_kind="buffered",
                rpt_id="IED-A/LLN0.brA",
                data_set_reference="IED-A/LLN0.dsA",
                target_indexes=[0],
                reason="fallback endpoint binding",
                source_classification="fallback",
            )
        ],
        uncovered_targets=[],
        planning_diagnostics=[],
        coverage=VerificationSubscriptionPlanCoverageSchema(
            total_targets=1,
            covered_targets=0,
            partially_covered_targets=1,
            uncovered_targets=0,
            groups_count=1,
            endpoints_count=1,
            planning_quality="fallback",
        ),
    )

    catalog = build_verification_plan_endpoint_catalog(plan)

    assert catalog is not None
    endpoint, _notes = catalog.resolve_transport_endpoint(
        ied_name="IED-A",
        access_point_name="P1",
        requested_host=None,
        requested_port=None,
    )
    assert endpoint.host == "10.10.10.250"
    assert endpoint.port == 12447
