from __future__ import annotations

import time
from collections.abc import Callable
from types import SimpleNamespace
from typing import cast

from app.services.external_ied_manual_control import (
    ExternalIedManualReportControlService,
    ExternalIedManualReportRequest,
)
from app.services.iec61850.client_control import Iec61850ClientControlService
from app.services.iec61850.report_runtime import Iec61850DeviceEndpoint, Iec61850ReportControlCandidate


class _RecordingControlService:
    instances: list["_RecordingControlService"] = []

    def __init__(self, **kwargs: object) -> None:
        self.kwargs: dict[str, object] = kwargs
        self.enable_calls: int = 0
        self.gi_calls: int = 0
        self.refresh_calls: int = 0
        self.disconnect_calls: int = 0
        self.close_calls: int = 0
        self.report_enabled: bool = True
        _RecordingControlService.instances.append(self)

    def enable_reporting(self) -> SimpleNamespace:
        self.enable_calls += 1
        return self._snapshot("enabled")

    def send_general_interrogation(self) -> SimpleNamespace:
        self.gi_calls += 1
        return self._snapshot("reporting", signal_states=[
            {
                "index": 0,
                "reference": "KINTE15BCU01CTRL1/XCBR1.Pos.stVal[ST]",
                "value": "true",
                "timestamp": "2026-07-04T18:00:00Z",
                "reason": "general-interrogation",
            },
        ])

    def refresh_reporting(self) -> SimpleNamespace:
        self.refresh_calls += 1
        return self._snapshot("reporting", signal_states=[
            {
                "index": 0,
                "reference": "KINTE15BCU01CTRL1/XCBR1.Pos.stVal[ST]",
                "value": "false",
                "timestamp": "2026-07-04T18:00:01Z",
                "reason": "data-change",
            },
        ])

    def _snapshot(self, status: str, signal_states: list[dict[str, object]] | None = None) -> SimpleNamespace:
        return SimpleNamespace(
            last_state=SimpleNamespace(runtime_status=SimpleNamespace(value=status), enabled=self.report_enabled),
            last_diagnostic=None,
            ui_state={"report": {
                "signal_states": signal_states or [],
                "values": [
                    {
                        "index": 0,
                        "reference": "KINTE15BCU01CTRL1/XCBR1$ST$Pos$stVal",
                        "data_reference": "KINTE15BCU01CTRL1/XCBR1$ST$Pos$stVal",
                        "value": "true",
                        "timestamp": "2026-07-04T18:00:00Z",
                        "reason": "general-interrogation",
                    },
                ] if signal_states else [],
            }},
        )

    def close_ied(self) -> None:
        self.close_calls += 1

    def disconnect_ied(self) -> None:
        self.disconnect_calls += 1

    def snapshot(self) -> SimpleNamespace:
        return self._snapshot("enabled" if self.report_enabled else "disabled")


_CONTROL_SERVICE_FACTORY = cast(
    Callable[..., Iec61850ClientControlService], _RecordingControlService
)


def test_manual_report_enable_uses_external_endpoint_and_rcb_reference():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(control_service_factory=_CONTROL_SERVICE_FACTORY, start_cleanup_thread=False)

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
    assert result.signal_states == ()
    assert len(_RecordingControlService.instances) == 1
    instance = _RecordingControlService.instances[0]
    assert instance.enable_calls == 1
    assert instance.gi_calls == 0
    endpoint = cast(Iec61850DeviceEndpoint, instance.kwargs["endpoint"])
    candidate = cast(Iec61850ReportControlCandidate, instance.kwargs["candidate"])
    assert endpoint.host == "172.16.40.128"
    assert endpoint.port == 12447
    assert candidate.id == "KINTE15BCU01CTRL1:LLN0$BR$brcbST"
    assert candidate.data_set_ref == "KINTE15BCU01CTRL1/LLN0$Events"


def test_manual_report_disable_closes_existing_session():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(control_service_factory=_CONTROL_SERVICE_FACTORY, start_cleanup_thread=False)
    request = ExternalIedManualReportRequest(
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        report_reference="KINTE15BCU01CTRL1/LLN0.BR.brcbST",
        report_kind="buffered",
    )

    _ = service.set_report_enabled(request, enabled=True)
    result = service.set_report_enabled(request, enabled=False)

    assert result.enabled is False
    assert result.status == "disabled"
    assert _RecordingControlService.instances[0].disconnect_calls == 1
    assert _RecordingControlService.instances[0].close_calls == 1


def test_manual_report_can_enable_multiple_reports_on_same_endpoint():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(control_service_factory=_CONTROL_SERVICE_FACTORY, start_cleanup_thread=False)
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


def test_manual_report_gi_returns_signal_values():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(control_service_factory=_CONTROL_SERVICE_FACTORY, start_cleanup_thread=False)
    request = ExternalIedManualReportRequest(
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        report_reference="KINTE15BCU01CTRL1/LLN0.BR.brcbST",
        report_kind="buffered",
    )

    enabled = service.set_report_enabled(request, enabled=True)
    result = service.send_general_interrogation(
        workspace_id=5,
        endpoint=request.endpoint,
        lease_id=str(enabled.lease_id),
    )

    assert result.enabled is True
    assert result.status == "reporting"
    assert _RecordingControlService.instances[0].gi_calls == 1
    assert result.signal_states == (
        {
            "index": 0,
            "reference": "KINTE15BCU01CTRL1/XCBR1.Pos.stVal[ST]",
            "value": "true",
            "timestamp": "2026-07-04T18:00:00Z",
            "reason": "general-interrogation",
        },
    )
    assert result.report_values == (
        {
            "index": 0,
            "reference": "KINTE15BCU01CTRL1/XCBR1$ST$Pos$stVal",
            "data_reference": "KINTE15BCU01CTRL1/XCBR1$ST$Pos$stVal",
            "value": "true",
            "timestamp": "2026-07-04T18:00:00Z",
            "reason": "general-interrogation",
        },
    )

def test_manual_report_heartbeat_renews_lease():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(
        control_service_factory=_CONTROL_SERVICE_FACTORY,
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
    assert renewed.renewed_at is not None
    assert enabled.renewed_at is not None
    assert renewed.renewed_at >= enabled.renewed_at
    assert renewed.signal_states == ()
    assert renewed.report_values == ()
    assert _RecordingControlService.instances[0].refresh_calls == 0
    assert _RecordingControlService.instances[0].close_calls == 0


def test_manual_report_poll_publishes_changed_values():
    _RecordingControlService.instances = []
    events: list[dict[str, object]] = []
    service = ExternalIedManualReportControlService(
        control_service_factory=_CONTROL_SERVICE_FACTORY,
        event_publisher=events.append,
        start_cleanup_thread=False,
    )
    request = ExternalIedManualReportRequest(
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        report_reference="KINTE15BCU01CTRL1/LLN0.BR.brcbST",
        report_kind="buffered",
    )

    enabled = service.set_report_enabled(request, enabled=True)
    published = service.poll_manual_report_values_once()
    duplicate = service.poll_manual_report_values_once()

    assert published == 1
    assert duplicate == 0
    assert _RecordingControlService.instances[0].refresh_calls == 2
    assert events == [
        {
            "channel": "external-ieds/manual-reports",
            "event": "external_ied_manual_report_values_changed",
            "workspace_id": 5,
            "endpoint": "172.16.40.128:12447",
            "ip": "172.16.40.128",
            "port": 12447,
            "report_reference": "KINTE15BCU01CTRL1/LLN0.BR.brcbST",
            "lease_id": enabled.lease_id,
            "status": "reporting",
            "signal_states": [
                {
                    "index": 0,
                    "reference": "KINTE15BCU01CTRL1/XCBR1.Pos.stVal[ST]",
                    "value": "false",
                    "timestamp": "2026-07-04T18:00:01Z",
                    "reason": "data-change",
                },
            ],
            "report_values": [
                {
                    "index": 0,
                    "reference": "KINTE15BCU01CTRL1/XCBR1$ST$Pos$stVal",
                    "data_reference": "KINTE15BCU01CTRL1/XCBR1$ST$Pos$stVal",
                    "value": "true",
                    "timestamp": "2026-07-04T18:00:00Z",
                    "reason": "general-interrogation",
                },
            ],
            "emitted_at": events[0]["emitted_at"],
        },
    ]


def test_manual_report_heartbeat_clears_disabled_session_state():
    _RecordingControlService.instances = []
    service = ExternalIedManualReportControlService(control_service_factory=_CONTROL_SERVICE_FACTORY, start_cleanup_thread=False)
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
    service = ExternalIedManualReportControlService(control_service_factory=_CONTROL_SERVICE_FACTORY, start_cleanup_thread=False)
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
        control_service_factory=_CONTROL_SERVICE_FACTORY,
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
        _ = service.set_report_enabled(request, enabled=True)
        time.sleep(0.4)
    finally:
        service.shutdown()

    assert _RecordingControlService.instances[0].disconnect_calls == 1
    assert _RecordingControlService.instances[0].close_calls == 1
