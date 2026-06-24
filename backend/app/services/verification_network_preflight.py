from __future__ import annotations

from dataclasses import dataclass, replace
import ipaddress
import re
from typing import Any

from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.signal_sheet.repository import SignalSheetRepository
from app.api.v1.signals.repository import SignalsRepository
from app.schemas.verification_schema import (
    VerificationAutoRunStartSchema,
    VerificationEvidenceDiagnosticSchema,
    VerificationNetworkPreflightGroupSchema,
    VerificationNetworkPreflightResponseSchema,
    VerificationNetworkPreflightSchema,
    VerificationNetworkInterfaceSchema,
)
from app.services.core_network_service import get_core_network_state
from app.services.iec61850.mms_adapter import build_mms_endpoint_catalog_from_scd_source
from app.services.iec61850.scl_import import Iec61850SqlAlchemySclImportRepository
from app.services.verification_endpoint_resolution import (
    build_verification_endpoint_resolution_diagnostic,
    resolve_verification_endpoint_resolution_policy,
)
from app.services.verification_planner import build_verification_subscription_plan, build_verification_target_sources
from app.services.verification_signal_endpoint_catalog import build_verification_signal_endpoint_catalog

_IP_PATTERN = re.compile(r"(?<!\d)(?:\d{1,3}\.){3}\d{1,3}(?!\d)")
_HOST_KEY_TOKENS = (
    "transport_host",
    "mms_host",
    "endpoint_host",
    "ied_host",
    "server_host",
    "host",
    "ip_address",
    "device_ip",
    "target_ip",
    "server_ip",
    "ip",
)


@dataclass(frozen=True, slots=True)
class _InterfaceObservation:
    interface_name: str
    local_ip: str
    netmask: str | None
    network: str | None

    def to_schema(self, *, matches_target: bool, recommended: bool, reason: str | None = None) -> VerificationNetworkInterfaceSchema:
        return VerificationNetworkInterfaceSchema(
            interface_name=self.interface_name,
            local_ip=self.local_ip,
            netmask=self.netmask,
            network=self.network,
            matches_target=matches_target,
            recommended=recommended,
            reason=reason,
        )


async def build_verification_network_preflight_response(
    *,
    workspace_id: int,
    payload: VerificationAutoRunStartSchema,
    db: AsyncSession,
) -> VerificationNetworkPreflightResponseSchema:
    selected_signal_ids = [int(signal_id) for signal_id in payload.signal_ids if int(signal_id) > 0]
    if not selected_signal_ids:
        raise ValueError("Verification requires at least one selected signal.")

    signals_repo = SignalsRepository(db)
    sheet_repo = SignalSheetRepository(db)
    if not await signals_repo.ensure_workspace(workspace_id):
        raise ValueError("Workspace not found")

    signals = await signals_repo.list_by_ids(workspace_id, selected_signal_ids)
    if len(signals) != len(selected_signal_ids):
        found_ids = {signal.id for signal in signals}
        missing_ids = [signal_id for signal_id in selected_signal_ids if signal_id not in found_ids]
        raise ValueError(f"Unknown or inactive signal_id values: {missing_ids}")

    allocation_rows = await sheet_repo.list_allocation_rows_by_signal_ids(workspace_id, selected_signal_ids)
    signals_by_id = {signal.id: signal for signal in signals}
    allocation_rows_by_signal_id = {row.signal_id: row for row in allocation_rows}
    sources = build_verification_target_sources(
        requested_signal_ids=selected_signal_ids,
        signals_by_id=signals_by_id,
        allocation_rows_by_signal_id=allocation_rows_by_signal_id,
    )
    subscription_plan = build_verification_subscription_plan(sources)

    requested_runtime_version = _normalize_runtime_mode(payload.execution_context.runtime_version)
    probe_execution_context = payload.execution_context.model_copy(update={"runtime_version": "mms"})
    active_runtime_selection = None
    loaded_runtime_scd_endpoint_catalog = None
    loaded_runtime_scd_available = False

    scl_repository = Iec61850SqlAlchemySclImportRepository(db)
    active_runtime_selection = await scl_repository.get_active_runtime_selection(workspace_id=workspace_id)
    if active_runtime_selection is not None:
        loaded_runtime_scd_available = True
        source_bytes = await scl_repository.get_import_source(
            workspace_id=workspace_id,
            import_id=active_runtime_selection.import_id,
        )
        if source_bytes is not None:
            loaded_runtime_scd_endpoint_catalog = build_mms_endpoint_catalog_from_scd_source(
                source_bytes,
                selected_ied=active_runtime_selection.selected_ied,
            )

    endpoint_resolution_policy = resolve_verification_endpoint_resolution_policy(
        execution_context=probe_execution_context,
        explicit_mms_endpoint_catalog=None,
        settings_mms_endpoint_catalog_json=None,
        loaded_runtime_scd_endpoint_catalog=loaded_runtime_scd_endpoint_catalog,
        loaded_runtime_scd_available=loaded_runtime_scd_available,
        active_runtime_selection_import_id=(
            active_runtime_selection.import_id if active_runtime_selection is not None else None
        ),
        active_runtime_selection_selected_ied=(
            active_runtime_selection.selected_ied if active_runtime_selection is not None else None
        ),
        active_runtime_selection_revision=(
            active_runtime_selection.runtime_revision if active_runtime_selection is not None else None
        ),
        transport_override_host=payload.execution_context.transport_override_host,
        transport_override_port=payload.execution_context.transport_override_port,
    )
    endpoint_resolution_diagnostic = build_verification_endpoint_resolution_diagnostic(endpoint_resolution_policy)

    derived_signal_catalog = build_verification_signal_endpoint_catalog(
        sources=sources,
        subscription_plan=subscription_plan,
    )
    effective_endpoint_catalog = endpoint_resolution_policy.endpoint_catalog or derived_signal_catalog
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = [endpoint_resolution_diagnostic]
    if effective_endpoint_catalog is not endpoint_resolution_policy.endpoint_catalog:
        endpoint_resolution_policy = replace(
            endpoint_resolution_policy,
            endpoint_catalog=effective_endpoint_catalog,
            transport_source="signal_list_fallback",
        )
        diagnostics.append(
            VerificationEvidenceDiagnosticSchema(
                code="network_preflight_signal_catalog",
                message="Signal list metadata supplied the MMS endpoint catalog fallback.",
                severity="info",
                details={
                    "group_count": len(subscription_plan.groups),
                },
            )
        )

    core_network_state = await _read_core_network_state()
    core_network_advisory = _build_core_network_advisory(core_network_state)
    diagnostics.extend(core_network_advisory)

    sources_by_signal_id = {source.signal_id: source for source in sources}
    network_interfaces = _build_network_interface_observations(core_network_state)

    group_responses: list[VerificationNetworkPreflightGroupSchema] = []
    group_states: list[str] = []

    for group in subscription_plan.groups:
        group_sources = [
            sources_by_signal_id[subscription_plan.targets[target_index].signal_id]
            for target_index in group.target_indexes
            if 0 <= target_index < len(subscription_plan.targets)
            and subscription_plan.targets[target_index].signal_id in sources_by_signal_id
        ]
        host_resolution = _resolve_group_target_host(
            group=group,
            group_sources=group_sources,
            endpoint_resolution_policy=endpoint_resolution_policy,
        )
        target_host = host_resolution["target_host"]
        target_port = host_resolution["target_port"]
        host_source = host_resolution["host_source"]
        observed_ips = host_resolution["observed_ips"]
        observed_hints = host_resolution["observed_hints"]
        group_diagnostics = list(host_resolution["diagnostics"])

        readiness_state, operator_hint, recommended = _resolve_group_readiness(
            target_host=target_host,
            target_port=target_port,
            host_source=host_source,
            core_network_state=core_network_state,
            interfaces=network_interfaces,
            matches=_match_interfaces(target_host, network_interfaces),
            requested_runtime_version=requested_runtime_version,
        )
        group_states.append(readiness_state)
        diagnostics.extend(group_diagnostics)

        group_responses.append(
            VerificationNetworkPreflightGroupSchema(
                group_id=group.group_id,
                endpoint_id=group.endpoint_id,
                ied_name=group.ied_name,
                access_point_name=group.access_point_name,
                target_host=target_host,
                target_port=target_port,
                readiness_state=readiness_state,
                operator_hint=operator_hint,
                recommended_interface_name=recommended.interface_name if recommended is not None else None,
                recommended_local_ip=recommended.local_ip if recommended is not None else None,
                recommended_netmask=recommended.netmask if recommended is not None else None,
                observed_ips=sorted(observed_ips),
                observed_network_hints=sorted(observed_hints),
                interfaces=[
                    interface.to_schema(
                        matches_target=interface.network is not None and target_host is not None and _host_matches_network(target_host, interface.network),
                        recommended=recommended is not None and interface.interface_name == recommended.interface_name,
                        reason=(
                            "matches target subnet"
                            if interface.network is not None and target_host is not None and _host_matches_network(target_host, interface.network)
                            else ("recommended local adapter" if recommended is not None and interface.interface_name == recommended.interface_name else None)
                        ),
                    )
                    for interface in network_interfaces
                ],
                diagnostics=group_diagnostics,
            )
        )

    overall_state = _resolve_overall_state(group_states, core_network_state)
    recommended_runtime_version = "mms" if overall_state == "ready" else "simulator"
    overall_hint = _resolve_overall_hint(
        overall_state=overall_state,
        groups=group_responses,
        requested_runtime_version=requested_runtime_version,
        recommended_runtime_version=recommended_runtime_version,
        core_network_state=core_network_state,
    )

    if overall_state == "ready":
        diagnostics.append(
            VerificationEvidenceDiagnosticSchema(
                code="network_preflight_ready",
                message="Agent-reported network state can reach the selected MMS target subnet.",
                severity="info",
                details={
                    "recommended_runtime_version": recommended_runtime_version,
                    "group_count": len(group_responses),
                },
            )
        )
    elif overall_state == "attention_required":
        diagnostics.append(
            VerificationEvidenceDiagnosticSchema(
                code="network_preflight_attention_required",
                message="Agent-reported network state does not currently match the selected MMS target subnet.",
                severity="warning",
                details={
                    "recommended_runtime_version": recommended_runtime_version,
                    "group_count": len(group_responses),
                },
            )
        )
    else:
        diagnostics.append(
            VerificationEvidenceDiagnosticSchema(
                code="network_preflight_unknown",
                message="Agent-reported network state is unavailable or incomplete.",
                severity="warning",
                details={
                    "recommended_runtime_version": recommended_runtime_version,
                    "group_count": len(group_responses),
                },
            )
        )

    return VerificationNetworkPreflightResponseSchema(
        preflight=VerificationNetworkPreflightSchema(
            workspace_id=workspace_id,
            test_run_id=payload.test_run_id,
            requested_runtime_version=requested_runtime_version,
            recommended_runtime_version=recommended_runtime_version,
            overall_state=overall_state,
            overall_hint=overall_hint,
            groups=group_responses,
            diagnostics=diagnostics,
        )
    )


async def build_verification_network_preflight_diagnostic(
    *,
    workspace_id: int,
    payload: VerificationAutoRunStartSchema,
    db: AsyncSession,
) -> VerificationEvidenceDiagnosticSchema:
    response = await build_verification_network_preflight_response(
        workspace_id=workspace_id,
        payload=payload,
        db=db,
    )
    preflight = response.preflight
    severity = "info" if preflight.overall_state == "ready" else "warning"
    return VerificationEvidenceDiagnosticSchema(
        code="network_preflight",
        message=preflight.overall_hint,
        severity=severity,
        details={
            "requested_runtime_version": preflight.requested_runtime_version,
            "recommended_runtime_version": preflight.recommended_runtime_version,
            "overall_state": preflight.overall_state,
            "group_count": len(preflight.groups),
        },
    )


async def _read_core_network_state() -> dict[str, Any] | None:
    try:
        return await get_core_network_state()
    except Exception:  # noqa: BLE001
        return None


def _build_core_network_advisory(core_network_state: dict[str, Any] | None) -> list[VerificationEvidenceDiagnosticSchema]:
    if core_network_state is None:
        return [
            VerificationEvidenceDiagnosticSchema(
                code="network_preflight_core_state_unavailable",
                message="Core network state is unavailable from the host agent; operator guidance is advisory only.",
                severity="warning",
                details={
                    "source": "redis/core_net:state",
                },
            )
        ]

    status = str(
        core_network_state.get("status")
        or core_network_state.get("state")
        or core_network_state.get("connection_status")
        or core_network_state.get("connectivity")
        or ""
    ).strip().lower()
    return [
        VerificationEvidenceDiagnosticSchema(
            code="network_preflight_core_state_observed",
            message="Core network state was observed from the host agent and used as advisory input.",
            severity="info" if status in {"online", "connected", "ready", "up"} else "warning",
            details={
                "status": status or None,
                "keys": sorted(str(key) for key in core_network_state.keys()),
            },
        )
    ]


def _build_network_interface_observations(core_network_state: dict[str, Any] | None) -> list[_InterfaceObservation]:
    if core_network_state is None:
        return []

    observations: list[_InterfaceObservation] = []
    interfaces = core_network_state.get("interfaces")
    if isinstance(interfaces, list):
        for item in interfaces:
            observation = _interface_from_mapping(item)
            if observation is not None:
                observations.append(observation)

    if observations:
        return observations

    active_interfaces = core_network_state.get("active_interfaces")
    ip_addresses = core_network_state.get("ip_addresses") or core_network_state.get("addresses")
    if isinstance(active_interfaces, list) and isinstance(ip_addresses, dict):
        for interface_name in active_interfaces:
            raw_addresses = ip_addresses.get(interface_name)
            for entry in _normalize_interface_address_entries(raw_addresses):
                observations.append(
                    _InterfaceObservation(
                        interface_name=str(interface_name),
                        local_ip=entry["local_ip"],
                        netmask=entry["netmask"],
                        network=entry["network"],
                    )
                )

    return observations


def _interface_from_mapping(value: Any) -> _InterfaceObservation | None:
    if not isinstance(value, dict):
        return None
    interface_name = _first_text(value, ("interface_name", "name", "iface", "device"))
    local_ip = _first_text(value, ("local_ip", "ip", "address", "ipv4", "ip_address"))
    netmask = _first_text(value, ("netmask", "mask"))
    network = _first_text(value, ("network", "subnet"))
    if not interface_name or not local_ip:
        return None
    if network is None and netmask is not None:
        try:
            network = str(ipaddress.ip_interface(f"{local_ip}/{netmask}").network)
        except ValueError:
            network = None
    return _InterfaceObservation(
        interface_name=interface_name,
        local_ip=local_ip,
        netmask=netmask,
        network=network,
    )


def _normalize_interface_address_entries(value: Any) -> list[dict[str, str | None]]:
    entries: list[dict[str, str | None]] = []
    if isinstance(value, str):
        entries.append({"local_ip": value.strip(), "netmask": None, "network": None})
        return entries
    if isinstance(value, dict):
        local_ip = _first_text(value, ("local_ip", "ip", "address", "ipv4", "ip_address"))
        netmask = _first_text(value, ("netmask", "mask"))
        network = _first_text(value, ("network", "subnet"))
        if local_ip:
            entries.append({"local_ip": local_ip, "netmask": netmask, "network": network})
        return entries
    if isinstance(value, list):
        for item in value:
            entries.extend(_normalize_interface_address_entries(item))
    return entries


def _first_text(mapping: dict[str, Any], keys: tuple[str, ...]) -> str | None:
    for key in keys:
        raw = mapping.get(key)
        if raw is None:
            continue
        text = str(raw).strip()
        if text:
            return text
    return None


def _resolve_group_target_host(
    *,
    group,
    group_sources,
    endpoint_resolution_policy,
) -> dict[str, Any]:
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = []
    observed_ips: set[str] = set()
    observed_hints: set[str] = set()
    target_host: str | None = None
    target_port: int | None = None
    host_source = "signal list metadata"

    for source in group_sources:
        _collect_network_metadata(source.signal_metadata, observed_ips=observed_ips, observed_hints=observed_hints)

    catalog = endpoint_resolution_policy.endpoint_catalog
    if catalog is not None and group.ied_name and group.access_point_name and group.access_point_name.lower() != "unknown":
        try:
            endpoint, notes = catalog.resolve_transport_endpoint(
                ied_name=group.ied_name,
                access_point_name=group.access_point_name,
                requested_host=endpoint_resolution_policy.transport_override_host,
                requested_port=endpoint_resolution_policy.transport_override_port,
            )
        except Exception as exc:  # noqa: BLE001
            diagnostics.append(
                VerificationEvidenceDiagnosticSchema(
                    code="network_preflight_endpoint_catalog_error",
                    message=str(exc),
                    severity="warning",
                    details={
                        "group_id": group.group_id,
                        "ied_name": group.ied_name,
                        "access_point_name": group.access_point_name,
                    },
                )
            )
        else:
            target_host = endpoint.host
            target_port = endpoint.port
            host_source = "endpoint catalog"
            diagnostics.append(
                VerificationEvidenceDiagnosticSchema(
                    code="network_preflight_endpoint_catalog_match",
                    message=f"Resolved MMS target {group.ied_name}/{group.access_point_name} from endpoint catalog.",
                    severity="info",
                    details={
                        "group_id": group.group_id,
                        "host": target_host,
                        "port": target_port,
                        "notes": list(notes),
                    },
                )
            )

    if target_host is None:
        candidates = _extract_target_host_candidates(group_sources)
        if candidates:
            target_host = candidates[0]
            target_port = 102
            host_source = "signal list metadata"
            diagnostics.append(
                VerificationEvidenceDiagnosticSchema(
                    code="network_preflight_signal_metadata_host",
                    message="Resolved an MMS target host from signal list metadata.",
                    severity="info",
                    details={
                        "group_id": group.group_id,
                        "host": target_host,
                        "candidates": candidates,
                    },
                )
            )

    if not observed_ips and target_host is not None:
        observed_ips.add(target_host)

    return {
        "target_host": target_host,
        "target_port": target_port,
        "host_source": host_source,
        "observed_ips": observed_ips,
        "observed_hints": observed_hints,
        "diagnostics": diagnostics,
    }


def _resolve_group_readiness(
    *,
    target_host: str | None,
    target_port: int | None,
    host_source: str,
    core_network_state: dict[str, Any] | None,
    interfaces: list[_InterfaceObservation],
    matches: list[_InterfaceObservation],
    requested_runtime_version: str,
) -> tuple[str, str, _InterfaceObservation | None]:
    target_label = _target_label(target_host, target_port)
    if target_host is None:
        if requested_runtime_version == "mms":
            return (
                "unknown",
                "No MMS target host could be resolved from the signal list yet. Load SCD or fill endpoint metadata before real MMS verification.",
                None,
            )
        return (
            "unknown",
            "Network preflight could not resolve an MMS target host yet.",
            None,
        )

    core_status = _resolve_core_network_status(core_network_state)
    if core_network_state is None:
        return (
            "unknown",
            f"Core network state is unavailable, so {target_label} can only be validated after the host agent publishes network readiness.",
            None,
        )

    if core_status in {"offline", "disconnected", "down", "absent", "disabled"}:
        return (
            "attention_required",
            f"The host agent reports the workstation network as {core_status or 'unavailable'}; connect the test host before real MMS verification.",
            _pick_recommended_interface(matches, interfaces),
        )

    if matches:
        recommended = matches[0]
        return (
            "ready",
            f"Host agent network state can reach {target_label}: {recommended.interface_name} ({recommended.local_ip}{'/' + recommended.netmask if recommended.netmask else ''}).",
            recommended,
        )

    if interfaces:
        recommended = interfaces[0]
        return (
            "attention_required",
            f"The host agent is publishing network state, but no active interface currently reaches {target_label}. Configure the workstation network or select a matching adapter before real MMS verification.",
            recommended,
        )

    if core_status in {"online", "connected", "ready", "up"}:
        return (
            "attention_required",
            f"The host agent reports the network as {core_status}; however, no usable interface was published for {target_label}.",
            None,
        )

    return (
        "unknown",
        f"The host agent network snapshot is incomplete for {target_label}; verification can still run in simulator mode.",
        None,
    )


def _pick_recommended_interface(matches: list[_InterfaceObservation], interfaces: list[_InterfaceObservation]) -> _InterfaceObservation | None:
    if matches:
        return matches[0]
    if interfaces:
        return interfaces[0]
    return None


def _resolve_core_network_status(core_network_state: dict[str, Any] | None) -> str | None:
    if core_network_state is None:
        return None
    for key in ("status", "state", "connection_status", "connectivity"):
        raw = core_network_state.get(key)
        if raw is None:
            continue
        text = str(raw).strip().lower()
        if text:
            return text
    return None


def _resolve_overall_state(group_states: list[str], core_network_state: dict[str, Any] | None) -> str:
    if not group_states:
        return "unknown"
    if any(state == "attention_required" for state in group_states):
        return "attention_required"
    if all(state == "ready" for state in group_states):
        return "ready"
    if core_network_state is None:
        return "unknown"
    return "attention_required" if any(state == "attention_required" for state in group_states) else "unknown"


def _resolve_overall_hint(
    *,
    overall_state: str,
    groups: list[VerificationNetworkPreflightGroupSchema],
    requested_runtime_version: str,
    recommended_runtime_version: str,
    core_network_state: dict[str, Any] | None,
) -> str:
    if overall_state == "ready":
        if requested_runtime_version == "mms":
            return "Host agent network readiness is sufficient for live MMS verification."
        return "Host agent network readiness is sufficient; switch the run to MMS to use the live BCU report path."

    if core_network_state is None:
        if groups:
            return "Host agent network state is unavailable; use simulator mode or wait until the agent publishes readiness."
        return "No verification targets were available for network preflight."

    for group in groups:
        if group.readiness_state == "attention_required":
            if recommended_runtime_version == "simulator":
                return f"{group.operator_hint} The run will fall back to simulator until the agent-reported network is ready."
            return group.operator_hint

    if groups:
        return "The host agent network snapshot is incomplete for the selected MMS target; review agent-reported state or signal metadata bindings."
    return "No verification targets were available for network preflight."


def _match_interfaces(
    target_host: str | None,
    interfaces: list[_InterfaceObservation],
) -> list[_InterfaceObservation]:
    if target_host is None:
        return []
    try:
        target_ip = ipaddress.ip_address(target_host)
    except ValueError:
        return []

    matches: list[_InterfaceObservation] = []
    for interface in interfaces:
        if interface.network is None:
            continue
        try:
            network = ipaddress.ip_network(interface.network, strict=False)
        except ValueError:
            continue
        if target_ip in network:
            matches.append(interface)
    return matches


def _host_matches_network(target_host: str, network: str) -> bool:
    try:
        target_ip = ipaddress.ip_address(target_host)
        candidate_network = ipaddress.ip_network(network, strict=False)
    except ValueError:
        return False
    return target_ip in candidate_network


def _collect_network_metadata(
    value: Any,
    *,
    observed_ips: set[str],
    observed_hints: set[str],
) -> None:
    if isinstance(value, dict):
        for key, child in value.items():
            key_text = str(key).strip().lower()
            if any(token in key_text for token in _HOST_KEY_TOKENS):
                if isinstance(child, str):
                    observed_hints.add(f"{key}={child}")
                    observed_ips.update(_extract_ipv4_strings(child))
                elif child is not None:
                    observed_hints.add(f"{key}={child}")
            _collect_network_metadata(child, observed_ips=observed_ips, observed_hints=observed_hints)
        return
    if isinstance(value, (list, tuple, set)):
        for item in value:
            _collect_network_metadata(item, observed_ips=observed_ips, observed_hints=observed_hints)
        return
    if isinstance(value, str):
        observed_ips.update(_extract_ipv4_strings(value))


def _extract_target_host_candidates(sources: list[Any]) -> list[str]:
    prioritized: list[str] = []
    fallback: list[str] = []
    for source in sources:
        metadata = getattr(source, "signal_metadata", None)
        if not isinstance(metadata, dict) or not metadata:
            continue
        keys = _extract_by_key_priority(metadata, _HOST_KEY_TOKENS)
        for candidate in keys:
            normalized = _normalize_ipv4_candidate(candidate)
            if normalized and normalized not in prioritized:
                prioritized.append(normalized)
        if not prioritized:
            for candidate in _extract_ipv4_strings(metadata):
                if candidate not in fallback:
                    fallback.append(candidate)
    return prioritized or fallback


def _extract_by_key_priority(payload: Any, key_tokens: tuple[str, ...]) -> list[str]:
    values: list[str] = []
    if isinstance(payload, dict):
        for key, child in payload.items():
            key_text = str(key).strip().lower()
            if any(token == key_text or token in key_text for token in key_tokens):
                values.extend(_extract_strings(child))
            values.extend(_extract_by_key_priority(child, key_tokens))
    elif isinstance(payload, (list, tuple, set)):
        for item in payload:
            values.extend(_extract_by_key_priority(item, key_tokens))
    return values


def _extract_strings(value: Any) -> list[str]:
    if isinstance(value, str):
        return [value]
    if isinstance(value, (list, tuple, set)):
        result: list[str] = []
        for item in value:
            result.extend(_extract_strings(item))
        return result
    if isinstance(value, dict):
        result: list[str] = []
        for child in value.values():
            result.extend(_extract_strings(child))
        return result
    return []


def _extract_ipv4_strings(value: Any) -> list[str]:
    candidates: list[str] = []
    for raw in _extract_strings(value):
        for match in _IP_PATTERN.findall(raw):
            normalized = _normalize_ipv4_candidate(match)
            if normalized and normalized not in candidates:
                candidates.append(normalized)
    return candidates


def _normalize_ipv4_candidate(value: str) -> str | None:
    text = str(value).strip()
    if not text:
        return None
    try:
        return str(ipaddress.ip_address(text))
    except ValueError:
        return None




def _normalize_runtime_mode(runtime_version: str | None) -> str:
    value = (runtime_version or "").strip().lower()
    if value in {"mms", "live", "live-mms", "real-mms"}:
        return "mms"
    return "simulator"

def _target_label(target_host: str | None, target_port: int | None) -> str:
    if target_host is None:
        return "unknown MMS target"
    if target_port is None:
        return target_host
    return f"{target_host}:{target_port}"
