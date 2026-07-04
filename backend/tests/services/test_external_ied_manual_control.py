from __future__ import annotations

import time
from types import SimpleNamespace

from app.services.external_ied_manual_control import (
    ExternalIedManualReportControlService,
    ExternalIedManualReportRequest,
)


class _RecordingControlService:
    instances: list["_RecordingControlService"] = []

    def __init__(self, **kwargs):
        self.kwargs = kwargs
        self.enable_calls = 0
        self.disconnect_calls = 0
        self.close_calls = 0
        self.report_enabled = True
        _RecordingControlService.instances.append(self)

    def enable_reporting(self):
        self.enable_calls += 1
        return SimpleNamespace(
            last_state=SimpleNamespace(runtime_status=SimpleNamespace(value="enabled")),
            last_diagnostic=None,
        )

    def close_ied(self):
        self.close_calls += 1

    def disconnect_ied(self):
        self.disconnect_calls += 1

    def snapshot(self):
        return SimpleNamespace(last_state=SimpleNamespace(enabled=self.report_enabled))


def test_manual_report_enable_uses_external_endpoint_and_rcb_reference():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(control_service_factory=_RecordingControlService, start_cleanup_thread=False)

    result = service.set_report_enabled(
        ExternalIedManualReportRequest(
            workspace_id=5,
            endpoint="172.16.40.128:12447",
            report_reference="KINTE15BCU01CTRL1/LLN0.BR.brcbST",
            report_name="brcbST",
            report_kind="buffered",
            dataset_reference="KINTE15BCU01CTRL1/LLN0$Events",
        ),
        enabled=True,
    )

    assert result.enabled is True
    assert result.status == "enabled"
    assert result.lease_id
    assert result.owner == "unitlab-manual-ied-inspector"
    assert result.expires_at
    assert len(_RecordingControlService.instances) == 1
    instance = _RecordingControlService.instances[0]
    assert instance.enable_calls == 1
    assert instance.kwargs["endpoint"].host == "172.16.40.128"
    assert instance.kwargs["endpoint"].port == 12447
    assert instance.kwargs["candidate"].id == "KINTE15BCU01CTRL1:LLN0$BR$brcbST"
    assert instance.kwargs["candidate"].data_set_ref == "KINTE15BCU01CTRL1/LLN0$Events"


def test_manual_report_disable_closes_existing_session():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(control_service_factory=_RecordingControlService, start_cleanup_thread=False)
    request = ExternalIedManualReportRequest(
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        report_reference="KINTE15BCU01CTRL1/LLN0.BR.brcbST",
        report_kind="buffered",
    )

    service.set_report_enabled(request, enabled=True)
    result = service.set_report_enabled(request, enabled=False)

    assert result.enabled is False
    assert result.status == "disabled"
    assert _RecordingControlService.instances[0].disconnect_calls == 1
    assert _RecordingControlService.instances[0].close_calls == 1


def test_manual_report_can_enable_multiple_reports_on_same_endpoint():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(control_service_factory=_RecordingControlService, start_cleanup_thread=False)
    first_request = ExternalIedManualReportRequest(
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        report_reference="KINTE15BCU01CTRL1/LLN0.BR.brcbST",
        report_kind="buffered",
    )
    second_request = ExternalIedManualReportRequest(
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        report_reference="KINTE15BCU01CTRL2/LLN0.BR.brcbST",
        report_kind="buffered",
    )

    first = service.set_report_enabled(first_request, enabled=True)
    second = service.set_report_enabled(second_request, enabled=True)

    assert first.enabled is True
    assert second.enabled is True
    assert first.lease_id != second.lease_id
    assert len(_RecordingControlService.instances) == 2
    assert _RecordingControlService.instances[0].close_calls == 0
    assert _RecordingControlService.instances[1].close_calls == 0


def test_manual_report_heartbeat_renews_lease():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(
        control_service_factory=_RecordingControlService,
        lease_ttl_seconds=1.0,
        start_cleanup_thread=False,
    )
    request = ExternalIedManualReportRequest(
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        report_reference="KINTE15BCU01CTRL1/LLN0.BR.brcbST",
        report_kind="buffered",
    )

    enabled = service.set_report_enabled(request, enabled=True)
    renewed = service.renew_lease(workspace_id=5, endpoint=request.endpoint, lease_id=str(enabled.lease_id))

    assert renewed.enabled is True
    assert renewed.lease_id == enabled.lease_id
    assert renewed.renewed_at >= enabled.renewed_at
    assert _RecordingControlService.instances[0].close_calls == 0


def test_manual_report_heartbeat_clears_disabled_session_state():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(control_service_factory=_RecordingControlService, start_cleanup_thread=False)
    request = ExternalIedManualReportRequest(
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        report_reference="KINTE15BCU01CTRL1/LLN0.BR.brcbST",
        report_kind="buffered",
    )

    enabled = service.set_report_enabled(request, enabled=True)
    _RecordingControlService.instances[0].report_enabled = False
    renewed = service.renew_lease(workspace_id=5, endpoint=request.endpoint, lease_id=str(enabled.lease_id))

    assert renewed.enabled is False
    assert renewed.status == "disabled"
    assert renewed.message == "report-disabled"
    assert _RecordingControlService.instances[0].disconnect_calls == 1
    assert _RecordingControlService.instances[0].close_calls == 1


def test_manual_report_release_by_lease_uses_cleanup_path():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(control_service_factory=_RecordingControlService, start_cleanup_thread=False)
    request = ExternalIedManualReportRequest(
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        report_reference="KINTE15BCU01CTRL1/LLN0.BR.brcbST",
        report_kind="buffered",
    )

    enabled = service.set_report_enabled(request, enabled=True)
    released = service.release_lease(workspace_id=5, endpoint=request.endpoint, lease_id=str(enabled.lease_id))

    assert released.enabled is False
    assert released.status == "disabled"
    assert _RecordingControlService.instances[0].disconnect_calls == 1
    assert _RecordingControlService.instances[0].close_calls == 1


def test_manual_report_expired_lease_is_cleaned_up():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(
        control_service_factory=_RecordingControlService,
        lease_ttl_seconds=0.05,
        cleanup_interval_seconds=0.05,
    )
    request = ExternalIedManualReportRequest(
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        report_reference="KINTE15BCU01CTRL1/LLN0.BR.brcbST",
        report_kind="buffered",
    )

    try:
        service.set_report_enabled(request, enabled=True)
        time.sleep(0.4)
    finally:
        service.shutdown()

    assert _RecordingControlService.instances[0].disconnect_calls == 1
    assert _RecordingControlService.instances[0].close_calls == 1
