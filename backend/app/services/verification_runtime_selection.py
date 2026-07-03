from __future__ import annotations

from contextlib import suppress
from dataclasses import dataclass
from datetime import datetime
import re
import time
from typing import Callable, Literal

from app.schemas.verification_schema import VerificationExecutionContextSchema
from app.services.iec61850.client_control import Iec61850ClientControlService
from app.services.iec61850.mms_adapter import Iec61850MmsEndpointCatalog
from app.services.iec61850.report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportControlReadResult,
    Iec61850ReportControlRef,
    Iec61850ReportControlState,
    Iec61850ReportEvent,
    Iec61850ReportRuntimeAdapter,
    Iec61850ReportRuntimeError,
    Iec61850ReportSession,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850RuntimeDiagnostic,
    Iec61850ReportControlCandidate,
    Iec61850RuntimeStatus,
    build_simulator_endpoint_for_plan_device,
    create_iec61850_simulator_adapter,
    compare_report_control_state,
    report_control_key,
    to_report_control_ref,
)


VerificationRuntimeMode = Literal["simulator", "mms"]
VerificationRuntimeTransportSource = Literal["simulator", "explicit_request", "settings_catalog", "loaded_scd", "validation_override", "signal_list_fallback", "unavailable"]
VerificationRuntimeModelSource = Literal["simulator", "loaded_scd", "discovery_fallback"]


@dataclass(frozen=True, slots=True)
class VerificationRuntimeSelection:
    runtime_mode: VerificationRuntimeMode
    adapter: Iec61850ReportRuntimeAdapter
    endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint]
    transport_source: VerificationRuntimeTransportSource
    model_source: VerificationRuntimeModelSource

    @property
    def runtime_source(self) -> VerificationRuntimeTransportSource:
        return self.transport_source


class _ClientControlMmsRuntimeAdapter:
    def __init__(
        self,
        *,
        client_id: str = "unitlab-backend-simulator",
        control_service_factory: Callable[..., Iec61850ClientControlService] = Iec61850ClientControlService,
    ) -> None:
        self._client_id = client_id
        self._control_service_factory = control_service_factory

    def connect(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidates: list[Iec61850ReportControlCandidate] | tuple[Iec61850ReportControlCandidate, ...],
    ) -> Iec61850ReportSession:
        if endpoint.mode.name != "MMS":
            raise Iec61850ReportRuntimeError(
                "UNSUPPORTED_ENDPOINT_MODE",
                "IEC 61850 MMS adapter only accepts MMS endpoints.",
            )
        return _ClientControlMmsSession(
            control_service_factory=self._control_service_factory,
            candidates=tuple(candidates),
            endpoint=endpoint,
            session_id=session_id,
            client_id=self._client_id,
        )


class _ClientControlMmsSession:
    def __init__(
        self,
        *,
        control_service_factory: Callable[..., Iec61850ClientControlService],
        candidates: tuple[Iec61850ReportControlCandidate, ...],
        endpoint: Iec61850DeviceEndpoint,
        session_id: str,
        client_id: str,
    ) -> None:
        self._control_service_factory = control_service_factory
        self._candidates = candidates
        self._endpoint = endpoint
        self._session_id = session_id
        self._client_id = client_id
        self._candidate_states: dict[str, dict[str, bool]] = {}
        self._discovery_service: Iec61850ClientControlService | None = None
        self._discovery_selected_candidate_key: str | None = None
        self._subscription_services: dict[str, Iec61850ClientControlService] = {}
        self._subscription_services_by_canonical_key: dict[str, Iec61850ClientControlService] = {}
        self._active_subscription_keys: set[str] = set()
        self._candidate_overrides: dict[str, Iec61850ReportControlCandidate] = {}

    def read_report_control(self, reference: Iec61850ReportControlRef) -> Iec61850ReportControlReadResult:
        requested_candidate = self._candidate_for_reference(reference)
        control_service = self._discovery_service_for_candidate(requested_candidate)
        discovery_snapshot = control_service.discover_ied()
        candidate = self._select_discovered_candidate_for_requested_candidate(
            requested_candidate=requested_candidate,
            discovery_snapshot=discovery_snapshot,
        )
        state = self._build_state(candidate, runtime_status="read")
        diagnostics = [
            *_discovery_summary_diagnostics(
                candidate=candidate,
                endpoint=self._endpoint,
                discovery_snapshot=discovery_snapshot,
            ),
            *_live_discovery_match_diagnostics(
                requested_candidate=requested_candidate,
                selected_candidate=candidate,
                discovery_snapshot=discovery_snapshot,
            ),
            *compare_report_control_state(candidate, state),
        ]
        return Iec61850ReportControlReadResult(
            endpoint=self._endpoint,
            candidate_id=candidate.id,
            state=state,
            diagnostics=tuple(diagnostics),
        )

    def reserve_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        candidate = self._candidate_for_reference(reference)
        self._candidate_state(candidate)["reserved"] = True
        return self._build_state(candidate, runtime_status="reserved", reserved_by=client_id)

    def release_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        candidate = self._candidate_for_reference(reference)
        control_service = self._subscription_service_for_candidate(candidate)
        release_report_control = getattr(control_service, "release_report_control", None)
        if callable(release_report_control):
            release_report_control()
        state = self._candidate_state(candidate)
        state["reserved"] = False
        state["enabled"] = False
        return self._build_state(candidate, runtime_status="released")

    def enable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        candidate = self._candidate_for_reference(reference)
        subscription_key = _canonical_live_subscription_key(candidate)
        if subscription_key in self._active_subscription_keys:
            state = self._candidate_state(candidate)
            state["enabled"] = True
            state["opened"] = True
            return self._build_state(candidate, runtime_status="enabled", enabled=True, reserved_by=client_id, owner=client_id)
        control_service = self._subscription_service_for_candidate(candidate)
        control_service.enable_reporting()
        state = self._candidate_state(candidate)
        state["enabled"] = True
        state["opened"] = True
        self._active_subscription_keys.add(subscription_key)
        self._subscription_services_by_canonical_key[subscription_key] = control_service
        return self._build_state(candidate, runtime_status="enabled", enabled=True, reserved_by=client_id, owner=client_id)

    def disable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        candidate = self._candidate_for_reference(reference)
        control_service = self._subscription_service_for_candidate(candidate)
        disable_report_control = getattr(control_service, "disable_report_control", None)
        if callable(disable_report_control):
            disable_report_control()
        self._candidate_state(candidate)["enabled"] = False
        return self._build_state(candidate, runtime_status="disabled")

    def send_general_interrogation(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportEvent:
        candidate = self._candidate_for_reference(reference)
        control_service = self._subscription_service_for_candidate(candidate)
        control_service.send_general_interrogation()
        snapshot = control_service.snapshot()
        report = snapshot.last_report
        if report is None:
            raise Iec61850ReportRuntimeError(
                "MMS_REPORT_NOT_OBSERVED",
                "IEC 61850 MMS client did not surface a report event.",
            )
        state = self._candidate_state(candidate)
        state["enabled"] = True
        state["opened"] = True
        return report

    def wait_for_report(
        self,
        reference: Iec61850ReportControlRef,
        client_id: str,
        *,
        after_sequence_number: int | None = None,
        after_event_id: str | None = None,
        timeout_ms: int = 5000,
    ) -> Iec61850ReportEvent:
        candidate = self._candidate_for_reference(reference)
        control_service = self._subscription_service_for_candidate(candidate)
        deadline = time.monotonic() + (max(1, int(timeout_ms)) / 1000)
        while True:
            snapshot = control_service.snapshot()
            report = snapshot.last_report
            if report is not None and self._is_new_report(
                report,
                after_sequence_number=after_sequence_number,
                after_event_id=after_event_id,
            ):
                return report
            if time.monotonic() >= deadline:
                raise Iec61850ReportRuntimeError(
                    "MMS_REPORT_TIMEOUT",
                    "IEC 61850 MMS client did not surface a new report event before timeout.",
                )
            time.sleep(0.05)

    def disconnect(self) -> None:
        if self._discovery_service is not None:
            with suppress(Exception):
                self._discovery_service.close_ied()
        for control_service in tuple(self._subscription_services.values()):
            with suppress(Exception):
                control_service.close_ied()
        self._discovery_service = None
        self._discovery_selected_candidate_key = None
        self._subscription_services.clear()
        self._subscription_services_by_canonical_key.clear()
        self._active_subscription_keys.clear()
        self._candidate_states.clear()
        self._candidate_overrides.clear()

    def _candidate_for_reference(self, reference: Iec61850ReportControlRef) -> Iec61850ReportControlCandidate:
        override = self._candidate_overrides.get(report_control_key(reference))
        if override is not None:
            return override
        for candidate in self._candidates:
            if to_report_control_ref(candidate) == reference:
                return candidate
        raise Iec61850ReportRuntimeError(
            "MMS_REPORT_CONTROL_MISMATCH",
            "IEC 61850 MMS adapter was asked to operate on an unexpected report control.",
        )

    def _discovery_service_for_candidate(self, candidate: Iec61850ReportControlCandidate) -> Iec61850ClientControlService:
        key = report_control_key(to_report_control_ref(candidate))
        if self._discovery_service is None:
            self._discovery_service = self._control_service_factory(
                session_id=self._session_id,
                client_id=self._client_id,
                endpoint=self._endpoint,
                candidate=candidate,
                available_candidates=self._available_candidates_for_service(candidate),
            )
            self._discovery_selected_candidate_key = key
            return self._discovery_service

        if self._discovery_selected_candidate_key != key:
            select_report_control = getattr(self._discovery_service, "select_report_control", None)
            if callable(select_report_control):
                select_report_control(candidate.id)
            self._discovery_selected_candidate_key = key
        return self._discovery_service

    def _subscription_service_for_candidate(self, candidate: Iec61850ReportControlCandidate) -> Iec61850ClientControlService:
        canonical_key = _canonical_live_subscription_key(candidate)
        canonical_service = self._subscription_services_by_canonical_key.get(canonical_key)
        if canonical_service is not None:
            return canonical_service
        key = report_control_key(to_report_control_ref(candidate))
        control_service = self._subscription_services.get(key)
        if control_service is not None:
            return control_service
        control_service = self._control_service_factory(
            session_id=f"{self._session_id}:{len(self._subscription_services)}",
            client_id=self._client_id,
            endpoint=self._endpoint,
            candidate=candidate,
            available_candidates=self._available_candidates_for_service(candidate),
        )
        self._prepare_subscription_service(control_service, candidate)
        self._subscription_services[key] = control_service
        return control_service

    def _prepare_subscription_service(
        self,
        control_service: Iec61850ClientControlService,
        candidate: Iec61850ReportControlCandidate,
    ) -> None:
        discover_ied = getattr(control_service, "discover_ied", None)
        if callable(discover_ied):
            if not self._seed_subscription_service_from_discovery(control_service):
                discover_ied()
        select_report_control = getattr(control_service, "select_report_control", None)
        if callable(select_report_control):
            select_report_control(candidate.id)

    def _seed_subscription_service_from_discovery(self, control_service: Iec61850ClientControlService) -> bool:
        discovery_service = self._discovery_service
        if discovery_service is None:
            return False
        discovered_rcbs = getattr(discovery_service, "_external_discovered_rcbs", None)
        if not isinstance(discovered_rcbs, list) or not discovered_rcbs:
            return False
        if not hasattr(control_service, "_external_discovered_rcbs"):
            return False
        setattr(control_service, "_external_discovered_rcbs", list(discovered_rcbs))
        setattr(control_service, "_external_discovered_rcbs_native", False)
        for attr in ("_external_live_discovery", "_last_discovery"):
            if hasattr(discovery_service, attr) and hasattr(control_service, attr):
                setattr(control_service, attr, getattr(discovery_service, attr))
        return True

    def _available_candidates_for_service(
        self,
        candidate: Iec61850ReportControlCandidate,
    ) -> tuple[Iec61850ReportControlCandidate, ...]:
        result: list[Iec61850ReportControlCandidate] = []
        seen: set[str] = set()
        for item in (candidate, *self._candidate_overrides.values(), *self._candidates):
            key = report_control_key(to_report_control_ref(item))
            if key in seen:
                continue
            seen.add(key)
            result.append(item)
        return tuple(result)

    def _select_discovered_candidate_for_requested_candidate(
        self,
        *,
        requested_candidate: Iec61850ReportControlCandidate,
        discovery_snapshot,
    ) -> Iec61850ReportControlCandidate:
        discovered_candidate = getattr(discovery_snapshot, "candidate", None)
        selected = (
            discovered_candidate
            if isinstance(discovered_candidate, Iec61850ReportControlCandidate)
            and not _candidate_needs_live_discovery(requested_candidate)
            else requested_candidate
        )
        selected_ref = _matching_discovered_rcb_ref(requested_candidate, discovery_snapshot)
        if selected_ref and self._discovery_service is not None:
            select_report_control = getattr(self._discovery_service, "select_report_control", None)
            if callable(select_report_control):
                selection_snapshot = select_report_control(selected_ref)
                selection_candidate = getattr(selection_snapshot, "candidate", None)
                if isinstance(selection_candidate, Iec61850ReportControlCandidate):
                    selected = selection_candidate
        elif selected is requested_candidate and _candidate_needs_live_discovery(requested_candidate):
            self._candidate_overrides.pop(report_control_key(to_report_control_ref(requested_candidate)), None)

        if selected is not requested_candidate and isinstance(selected, Iec61850ReportControlCandidate):
            self._candidate_overrides[report_control_key(to_report_control_ref(requested_candidate))] = selected
            self._discovery_selected_candidate_key = report_control_key(to_report_control_ref(selected))
            return selected
        return requested_candidate

    def _candidate_state(self, candidate: Iec61850ReportControlCandidate) -> dict[str, bool]:
        key = report_control_key(to_report_control_ref(candidate))
        return self._candidate_states.setdefault(key, {"opened": False, "reserved": False, "enabled": False})

    def _build_state(
        self,
        candidate: Iec61850ReportControlCandidate,
        *,
        runtime_status: str,
        enabled: bool | None = None,
        reserved_by: str | None = None,
        owner: str | None = None,
    ) -> Iec61850ReportControlState:
        state = self._candidate_state(candidate)
        status = {
            "read": Iec61850RuntimeStatus.READ,
            "reserved": Iec61850RuntimeStatus.RESERVED,
            "enabled": Iec61850RuntimeStatus.ENABLED,
            "disabled": Iec61850RuntimeStatus.DISABLED,
            "released": Iec61850RuntimeStatus.RELEASED,
        }.get(runtime_status, Iec61850RuntimeStatus.CONNECTED)
        return Iec61850ReportControlState(
            reference=to_report_control_ref(candidate),
            runtime_status=status,
            rpt_id=candidate.rpt_id,
            data_set_ref=candidate.data_set_ref,
            conf_rev=candidate.conf_rev,
            indexed=candidate.indexed,
            buffer_time_ms=candidate.buffer_time_ms,
            integrity_period_ms=candidate.integrity_period_ms,
            trigger_options=candidate.trigger_options,
            optional_fields=candidate.optional_fields,
            signal_count=candidate.signal_count,
            enabled=state["enabled"] if enabled is None else enabled,
            reserved_by=reserved_by,
            owner=owner,
            sequence_number=0,
            gi_in_progress=False,
        )

    @staticmethod
    def _is_new_report(
        report: Iec61850ReportEvent,
        *,
        after_sequence_number: int | None,
        after_event_id: str | None,
    ) -> bool:
        if after_event_id and report.id == after_event_id:
            return False
        if after_sequence_number is not None and report.sequence_number is not None:
            return report.sequence_number > after_sequence_number
        return True


def _candidate_needs_live_discovery(candidate: Iec61850ReportControlCandidate) -> bool:
    return (
        not candidate.logical_device_inst.strip()
        or not candidate.logical_node_name.strip()
        or not candidate.report_control_name.strip()
        or candidate.data_set_ref is None
        or candidate.conf_rev is None
    )


def _canonical_live_subscription_key(candidate: Iec61850ReportControlCandidate) -> str:
    data_set_ref = str(candidate.data_set_ref or "").strip()
    if data_set_ref:
        return f"dataset:{data_set_ref.lower()}"
    return "report:" + report_control_key(to_report_control_ref(candidate)).lower()


def _discovery_summary_diagnostics(
    *,
    candidate: Iec61850ReportControlCandidate,
    endpoint: Iec61850DeviceEndpoint,
    discovery_snapshot,
) -> tuple[Iec61850RuntimeDiagnostic, ...]:
    ui_state = getattr(discovery_snapshot, "ui_state", None)
    if not isinstance(ui_state, dict):
        return ()
    discovery = ui_state.get("discovery")
    if not isinstance(discovery, dict) or not bool(discovery.get("discovered")):
        return ()

    logical_devices = _int_from_discovery(discovery, "logical_devices")
    report_controls = _int_from_discovery(discovery, "report_controls")
    data_sets = _int_from_discovery(discovery, "data_sets")
    data_set_members = _int_from_discovery(discovery, "data_set_members")
    logical_nodes = _int_from_discovery(discovery, "logical_nodes")
    signals = _int_from_discovery(discovery, "signals")
    return (
        Iec61850RuntimeDiagnostic(
            severity="info",
            code="MMS_DISCOVERY_SUMMARY",
            message=(
                "IEC 61850 MMS discovery completed: "
                f"{logical_devices} logical devices, {report_controls} report controls."
            ),
            reference=to_report_control_ref(candidate),
            details={
                "endpoint_id": endpoint.id,
                "endpoint_host": endpoint.host,
                "endpoint_port": endpoint.port,
                "logical_devices": logical_devices,
                "logical_nodes": logical_nodes,
                "data_sets": data_sets,
                "data_set_members": data_set_members,
                "report_controls": report_controls,
                "signals": signals,
            },
        ),
    )


def _live_discovery_match_diagnostics(
    *,
    requested_candidate: Iec61850ReportControlCandidate,
    selected_candidate: Iec61850ReportControlCandidate,
    discovery_snapshot,
) -> tuple[Iec61850RuntimeDiagnostic, ...]:
    if not _candidate_needs_live_discovery(requested_candidate):
        return ()
    if selected_candidate is not requested_candidate:
        return ()
    if _matching_discovered_rcb_ref(requested_candidate, discovery_snapshot):
        return ()
    return (
        Iec61850RuntimeDiagnostic(
            severity="error",
            code="MMS_REPORT_CONTROL_NOT_MATCHED",
            message="IEC 61850 discovery did not find a ReportControl dataset containing the requested signal-list addresses.",
            reference=to_report_control_ref(requested_candidate),
            details={
                "candidate_id": requested_candidate.id,
                "signal_count": requested_candidate.signal_count,
            },
        ),
    )


def _int_from_discovery(discovery: dict, key: str) -> int:
    value = discovery.get(key)
    if isinstance(value, bool):
        return 0
    if isinstance(value, int):
        return max(0, value)
    if isinstance(value, float):
        return max(0, int(value))
    return 0


def _matching_discovered_rcb_ref(
    requested_candidate: Iec61850ReportControlCandidate,
    discovery_snapshot,
) -> str | None:
    if not _candidate_needs_live_discovery(requested_candidate):
        return None
    discovery = getattr(discovery_snapshot, "last_discovery", None)
    if not isinstance(discovery, dict):
        return None
    desired_references = tuple(signal.reference for signal in requested_candidate.signals if signal.reference)
    if not desired_references:
        return None

    data_set_members_by_ref = _data_set_members_by_reference(discovery)
    available_by_data_set = _available_report_controls_by_dataset(discovery_snapshot)
    report_controls = discovery.get("reportControls")
    if not isinstance(report_controls, list):
        return None
    best_match: tuple[int, str] | None = None
    for report_control in report_controls:
        if not isinstance(report_control, dict):
            continue
        data_set_ref = report_control.get("dataSetRef")
        if not isinstance(data_set_ref, str) or not data_set_ref:
            continue
        members = data_set_members_by_ref.get(data_set_ref, ())
        score = _report_control_match_score(report_control, members, desired_references)
        if score <= 0:
            continue
        available = available_by_data_set.get(data_set_ref)
        if available is not None:
            for key in ("report_control_id", "rcb_ref"):
                value = available.get(key)
                if isinstance(value, str) and value.strip():
                    if best_match is None or score > best_match[0]:
                        best_match = (score, value.strip())
                    break
            else:
                value = None
            if best_match is not None and best_match[0] == score:
                continue
        value = report_control.get("id")
        if isinstance(value, str) and value.strip():
            if best_match is None or score > best_match[0]:
                best_match = (score, value.strip())
    return best_match[1] if best_match is not None else None


def _data_set_members_by_reference(discovery: dict) -> dict[str, tuple[str, ...]]:
    data_sets = discovery.get("dataSets")
    if not isinstance(data_sets, list):
        return {}
    result: dict[str, tuple[str, ...]] = {}
    for data_set in data_sets:
        if not isinstance(data_set, dict):
            continue
        reference = data_set.get("reference")
        members = data_set.get("members")
        if not isinstance(reference, str) or not isinstance(members, list):
            continue
        member_refs = []
        for member in members:
            if not isinstance(member, dict):
                continue
            for key in ("mmsReference", "reference"):
                value = member.get(key)
                if isinstance(value, str) and value.strip():
                    member_refs.append(value.strip())
        result[reference] = tuple(member_refs)
    return result


def _available_report_controls_by_dataset(discovery_snapshot) -> dict[str, dict]:
    ui_state = getattr(discovery_snapshot, "ui_state", None)
    if not isinstance(ui_state, dict):
        return {}
    discovery = ui_state.get("discovery")
    if not isinstance(discovery, dict):
        return {}
    available = discovery.get("available_report_controls")
    if not isinstance(available, list):
        return {}
    result: dict[str, dict] = {}
    for item in available:
        if not isinstance(item, dict):
            continue
        data_set_ref = item.get("data_set_ref")
        if isinstance(data_set_ref, str) and data_set_ref.strip():
            result.setdefault(data_set_ref.strip(), item)
    return result


def _dataset_contains_any_requested_signal(
    members: tuple[str, ...],
    desired_references: tuple[str, ...],
) -> bool:
    for desired in desired_references:
        for member in members:
            if _signal_reference_matches(desired, member):
                return True
    return False


def _report_control_match_score(report_control: dict, members: tuple[str, ...], desired_references: tuple[str, ...]) -> int:
    if _dataset_contains_any_requested_signal(members, desired_references):
        return 100
    if not _report_control_matches_requested_domain(report_control, desired_references):
        return 0
    desired_constraints = _functional_constraints_from_signal_references(desired_references)
    report_constraints = _functional_constraints_from_report_control(report_control)
    if desired_constraints and report_constraints:
        return 20 if desired_constraints & report_constraints else 0
    return 10


def _functional_constraints_from_signal_references(references: tuple[str, ...]) -> set[str]:
    result: set[str] = set()
    for reference in references:
        bracket_match = re.search(r"\[([A-Za-z0-9]+)\]\s*$", str(reference))
        if bracket_match is not None:
            result.add(bracket_match.group(1).upper())
            continue
        value = str(reference).split("!", 1)[-1]
        if "/" in value:
            value = value.split("/", 1)[1]
        parts = [part.strip() for part in value.split("$") if part.strip()]
        if len(parts) >= 2 and re.fullmatch(r"[A-Za-z]{2}", parts[1]):
            result.add(parts[1].upper())
    return result


def _functional_constraints_from_report_control(report_control: dict) -> set[str]:
    result: set[str] = set()
    for key in ("dataSetRef", "item", "name", "id", "rptId"):
        value = report_control.get(key)
        if not isinstance(value, str) or not value.strip():
            continue
        normalized = value.upper()
        for constraint in ("ST", "MX"):
            if re.search(rf"(?:^|[^A-Z0-9]){constraint}(?:[^A-Z0-9]|$)", normalized):
                result.add(constraint)
            elif re.search(rf"RPT{constraint}DS", normalized):
                result.add(constraint)
            elif re.search(rf"BRCB{constraint}|URCB{constraint}", normalized):
                result.add(constraint)
    return result


def _report_control_matches_requested_domain(report_control: dict, desired_references: tuple[str, ...]) -> bool:
    domain = str(report_control.get("domain") or "").strip().lower()
    data_set_ref = str(report_control.get("dataSetRef") or "").strip().lower()
    if not domain and "/" in data_set_ref:
        domain = data_set_ref.split("/", 1)[0]
    if not domain:
        return False
    return any(str(desired).strip().lower().startswith(domain + "/") for desired in desired_references)


def _signal_reference_matches(desired: str, discovered_member: str) -> bool:
    desired_canonical = _canonical_signal_reference(desired)
    member_canonical = _canonical_signal_reference(discovered_member)
    if not desired_canonical or not member_canonical:
        return False
    if desired_canonical == member_canonical:
        return True
    if desired_canonical.startswith(member_canonical + "."):
        return True
    desired_compact = _compact_signal_reference(desired_canonical)
    member_compact = _compact_signal_reference(member_canonical)
    return bool(member_compact and (desired_compact == member_compact or desired_compact.endswith(member_compact)))


def _canonical_signal_reference(reference: str) -> str:
    value = reference.strip()
    if not value:
        return ""
    if "!" in value:
        value = value.split("!", 1)[1]
    if "$" in value:
        domain, separator, item = value.partition("/")
        parts = item.split("$") if separator else value.split("$")
        if len(parts) >= 3:
            logical_node = parts[0]
            data_parts = parts[2:]
            value = f"{domain}/{logical_node}.{'.'.join(data_parts)}" if separator else f"{logical_node}.{'.'.join(data_parts)}"
        else:
            value = value.replace("$", ".")
    if value.endswith("]") and "[" in value:
        value = value[: value.rfind("[")]
    return value.replace("/", ".").strip(".").lower()


def _compact_signal_reference(reference: str) -> str:
    return "".join(character for character in reference.lower() if character.isalnum())


def resolve_verification_runtime(
    *,
    execution_context: VerificationExecutionContextSchema,
    now: Callable[[], datetime] | None = None,
    endpoint_catalog: Iec61850MmsEndpointCatalog | None = None,
    transport_source: VerificationRuntimeTransportSource | None = None,
    model_source: VerificationRuntimeModelSource | None = None,
    transport_override_host: str | None = None,
    transport_override_port: int | None = None,
    simulator_endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device,
    mms_control_service_factory: Callable[..., Iec61850ClientControlService] = Iec61850ClientControlService,
) -> VerificationRuntimeSelection:
    runtime_mode = _normalize_runtime_mode(execution_context.runtime_version)
    if runtime_mode == "mms":
        if endpoint_catalog is None:
            raise ValueError("MMS runtime requires an endpoint catalog.")
        if transport_override_host is not None and transport_override_host.strip() or (transport_override_port is not None and transport_override_port > 0):
            override_host = transport_override_host.strip() if transport_override_host is not None and transport_override_host.strip() else None
            override_port = transport_override_port if transport_override_port is not None and transport_override_port > 0 else None

            def _endpoint_for_device_with_override(device: Iec61850ReportSubscriptionPlanDevice) -> Iec61850DeviceEndpoint:
                endpoint, _notes = endpoint_catalog.resolve_transport_endpoint(
                    ied_name=device.ied_name,
                    access_point_name=device.access_point_name,
                    requested_host=override_host,
                    requested_port=override_port,
                )
                return endpoint

            endpoint_for_device = _endpoint_for_device_with_override
            resolved_transport_source: VerificationRuntimeTransportSource = "validation_override"
        else:
            endpoint_for_device = endpoint_catalog.endpoint_for_plan_device
            resolved_transport_source = transport_source or "explicit_request"
        return VerificationRuntimeSelection(
            runtime_mode="mms",
            adapter=_ClientControlMmsRuntimeAdapter(control_service_factory=mms_control_service_factory),
            endpoint_for_device=endpoint_for_device,
            transport_source=resolved_transport_source,
            model_source=model_source or "discovery_fallback",
        )

    return VerificationRuntimeSelection(
        runtime_mode="simulator",
        adapter=create_iec61850_simulator_adapter(now=now),
        endpoint_for_device=simulator_endpoint_for_device,
        transport_source="simulator",
        model_source="simulator",
    )


def _normalize_runtime_mode(runtime_version: str | None) -> VerificationRuntimeMode:
    value = (runtime_version or "").strip().lower()
    if value in {"mms", "live", "live-mms", "real-mms"}:
        return "mms"
    return "simulator"
