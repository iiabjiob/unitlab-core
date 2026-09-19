from __future__ import annotations

import pytest

from app.schemas.verification_schema import VerificationExecutionContextSchema
from app.services.iec61850.mms_adapter import Iec61850MmsEndpointCatalogEntry, build_mms_endpoint_catalog, build_mms_endpoint_catalog_from_scd_source
from app.services.iec61850.report_runtime import Iec61850ReportRuntimeError
from app.services.verification_endpoint_resolution import (
    build_verification_endpoint_resolution_diagnostic,
    resolve_verification_endpoint_resolution_policy,
)


def _build_context(runtime_version: str) -> VerificationExecutionContextSchema:
    return VerificationExecutionContextSchema(
        project_id=1,
        signal_list_revision_id=2,
        planner_version="planner",
        runtime_version=runtime_version,
        policy_version="v1",
    )


def test_resolve_verification_endpoint_policy_defaults_to_simulator() -> None:
    policy = resolve_verification_endpoint_resolution_policy(
        execution_context=_build_context("simulator"),
    )

    assert policy.runtime_mode == "simulator"
    assert policy.transport_source == "simulator"
    assert policy.model_source == "simulator"
    assert policy.endpoint_catalog is None

    diagnostic = build_verification_endpoint_resolution_diagnostic(policy)
    assert diagnostic.code == "endpoint_resolution_policy"
    assert diagnostic.severity == "info"
    assert diagnostic.details is not None
    assert diagnostic.details["runtime_mode"] == "simulator"


def test_resolve_verification_endpoint_policy_uses_explicit_catalog_for_transport() -> None:
    catalog = build_mms_endpoint_catalog(
        (
            Iec61850MmsEndpointCatalogEntry(
                ied_name="IED-A",
                access_point_name="P1",
                host="10.10.10.250",
                port=12447,
            ),
        )
    )

    policy = resolve_verification_endpoint_resolution_policy(
        execution_context=_build_context("mms"),
        explicit_mms_endpoint_catalog=catalog,
    )

    assert policy.runtime_mode == "mms"
    assert policy.transport_source == "explicit_request"
    assert policy.model_source == "discovery_fallback"
    assert policy.endpoint_catalog is catalog

    diagnostic = build_verification_endpoint_resolution_diagnostic(policy)
    assert diagnostic.details is not None
    assert diagnostic.details["transport_source"] == "explicit_request"
    assert diagnostic.details["model_source"] == "discovery_fallback"


def test_resolve_verification_endpoint_policy_uses_settings_catalog_when_explicit_catalog_is_missing() -> None:
    policy = resolve_verification_endpoint_resolution_policy(
        execution_context=_build_context("mms"),
        settings_mms_endpoint_catalog_json='[{"ied_name":"IED-A","access_point_name":"P1","host":"10.10.10.250","port":12447}]',
    )

    assert policy.transport_source == "settings_catalog"
    assert policy.endpoint_catalog is not None

    diagnostic = build_verification_endpoint_resolution_diagnostic(policy)
    assert diagnostic.details is not None
    assert diagnostic.details["transport_source"] == "settings_catalog"


def test_resolve_verification_endpoint_policy_marks_loaded_scd_model_binding() -> None:
    policy = resolve_verification_endpoint_resolution_policy(
        execution_context=_build_context("mms"),
        active_runtime_selection_import_id="import-7",
        active_runtime_selection_selected_ied="IED-A",
        active_runtime_selection_revision=12,
    )

    assert policy.model_source == "loaded_scd"
    assert policy.selected_runtime_import_id == "import-7"
    assert policy.selected_runtime_selected_ied == "IED-A"
    assert policy.selected_runtime_revision == 12

    diagnostic = build_verification_endpoint_resolution_diagnostic(policy)
    assert diagnostic.details is not None
    assert diagnostic.details["model_source"] == "loaded_scd"
    assert diagnostic.details["selected_runtime_import_id"] == "import-7"
    assert diagnostic.details["selected_runtime_selected_ied"] == "IED-A"


def test_resolve_verification_endpoint_policy_rejects_invalid_settings_catalog_json() -> None:
    with pytest.raises(Iec61850ReportRuntimeError) as error:
        _ = resolve_verification_endpoint_resolution_policy(
            execution_context=_build_context("mms"),
            settings_mms_endpoint_catalog_json="not-json",
        )

    assert error.value.code == "INVALID_MMS_ENDPOINT_CATALOG"


def test_resolve_verification_endpoint_policy_uses_validation_override_for_transport() -> None:
    catalog = build_mms_endpoint_catalog(
        (
            Iec61850MmsEndpointCatalogEntry(
                ied_name="IED-A",
                access_point_name="P1",
                host="10.10.10.250",
                port=12447,
            ),
        )
    )

    policy = resolve_verification_endpoint_resolution_policy(
        execution_context=_build_context("mms"),
        explicit_mms_endpoint_catalog=catalog,
        transport_override_host="10.10.10.99",
        transport_override_port=12447,
    )

    assert policy.transport_source == "validation_override"
    assert policy.transport_override_host == "10.10.10.99"
    assert policy.transport_override_port == 12447

    diagnostic = build_verification_endpoint_resolution_diagnostic(policy)
    assert diagnostic.details is not None
    assert diagnostic.details["transport_source"] == "validation_override"
    assert diagnostic.details["transport_override_host"] == "10.10.10.99"


def test_resolve_verification_endpoint_policy_uses_loaded_scd_for_transport() -> None:
    catalog = build_mms_endpoint_catalog_from_scd_source(
        b"<SCL><Communication><SubNetwork type='8-MMS'>"
        + b"<ConnectedAP apName='P1' iedName='IED-A'><Address><P type='IP'>10.10.10.250</P></Address></ConnectedAP>"
        + b"</SubNetwork></Communication></SCL>",
        selected_ied='IED-A',
    )

    policy = resolve_verification_endpoint_resolution_policy(
        execution_context=_build_context("mms"),
        loaded_runtime_scd_endpoint_catalog=catalog,
        loaded_runtime_scd_available=True,
        active_runtime_selection_import_id='import-7',
        active_runtime_selection_selected_ied='IED-A',
        active_runtime_selection_revision=12,
    )

    assert policy.transport_source == 'loaded_scd'
    assert policy.endpoint_catalog is not None
    diagnostic = build_verification_endpoint_resolution_diagnostic(policy)
    assert diagnostic.details is not None
    assert diagnostic.details['transport_source'] == 'loaded_scd'


def test_build_mms_endpoint_catalog_from_scd_source_extracts_connected_ap_hosts() -> None:
    catalog = build_mms_endpoint_catalog_from_scd_source(
        b"<SCL><Communication><SubNetwork type='8-MMS'>"
        + b"<ConnectedAP apName='P1' iedName='IED-A'><Address><P type='IP'>10.10.10.250</P></Address></ConnectedAP>"
        + b"<ConnectedAP apName='P2' iedName='IED-B'><Address><P type='IP'>10.10.10.251</P></Address></ConnectedAP>"
        + b"</SubNetwork></Communication></SCL>",
        selected_ied='IED-A',
    )

    assert catalog is not None
    endpoint, notes = catalog.resolve_transport_endpoint(
        ied_name='IED-A',
        access_point_name='P1',
        requested_host=None,
        requested_port=None,
    )
    assert endpoint.id == 'mms:IED-A/P1@10.10.10.250:102'
    assert 'transport host resolved from endpoint catalog' in notes
    with pytest.raises(Iec61850ReportRuntimeError):
        _ = catalog.resolve_transport_endpoint(
            ied_name='IED-B',
            access_point_name='P2',
            requested_host=None,
            requested_port=None,
        )
