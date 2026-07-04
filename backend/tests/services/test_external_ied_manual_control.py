from __future__ import annotations

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
        self.close_calls = 0
        _RecordingControlService.instances.append(self)

    def enable_reporting(self):
        self.enable_calls += 1
        return SimpleNamespace(
            last_state=SimpleNamespace(runtime_status=SimpleNamespace(value="enabled")),
            last_diagnostic=None,
        )

    def close_ied(self):
        self.close_calls += 1


def test_manual_report_enable_uses_external_endpoint_and_rcb_reference():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(control_service_factory=_RecordingControlService)

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
    assert len(_RecordingControlService.instances) == 1
    instance = _RecordingControlService.instances[0]
    assert instance.enable_calls == 1
    assert instance.kwargs["endpoint"].host == "172.16.40.128"
    assert instance.kwargs["endpoint"].port == 12447
    assert instance.kwargs["candidate"].id == "KINTE15BCU01CTRL1:LLN0$BR$brcbST"
    assert instance.kwargs["candidate"].data_set_ref == "KINTE15BCU01CTRL1/LLN0$Events"


def test_manual_report_disable_closes_existing_session():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(control_service_factory=_RecordingControlService)
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
    assert _RecordingControlService.instances[0].close_calls == 1

