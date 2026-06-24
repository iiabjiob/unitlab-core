from __future__ import annotations

from app.schemas.verification_schema import (
    VerificationSubscriptionPlanCoverageSchema,
    VerificationSubscriptionPlanGroupSchema,
    VerificationSubscriptionPlanSchema,
)
from app.services.verification_planner import VerificationTargetSource
from app.services.verification_signal_endpoint_catalog import build_verification_signal_endpoint_catalog


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
