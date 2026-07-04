from __future__ import annotations

import hashlib
import json
import time
from dataclasses import dataclass
from typing import Any, Protocol

from app.core.logger import get_logger
from app.services.external_ied_discovery_scheduler import (
    DiscoveryCacheMetadata,
    ExternalIedDiscoveryRequest,
)
from app.services.iec61850.client_control import (
    Iec61850ClientControlService,
    Iec61850ClientTargetRequest,
)

logger = get_logger("external_ied_discovery")


@dataclass(frozen=True, slots=True)
class DiscoverySummary:
    logical_devices: int = 0
    logical_nodes: int = 0
    datasets: int = 0
    dataset_members: int = 0
    report_controls: int = 0
    signals: int = 0
    duration_ms: int = 0

    def to_payload(self) -> dict[str, int]:
        return {
            "logical_devices": self.logical_devices,
            "logical_nodes": self.logical_nodes,
            "datasets": self.datasets,
            "dataset_members": self.dataset_members,
            "report_controls": self.report_controls,
            "signals": self.signals,
            "duration_ms": self.duration_ms,
        }


@dataclass(frozen=True, slots=True)
class DiscoveryResult:
    request: ExternalIedDiscoveryRequest
    metadata: DiscoveryCacheMetadata
    summary: DiscoverySummary
    model: dict[str, Any]


class ExternalIedDiscoveryEngine(Protocol):
    def discover(self, request: ExternalIedDiscoveryRequest) -> DiscoveryResult: ...


class MmsExternalIedDiscoveryEngine:
    def __init__(
        self,
        *,
        control_service_factory=Iec61850ClientControlService,
    ) -> None:
        self._control_service_factory = control_service_factory

    def discover(self, request: ExternalIedDiscoveryRequest) -> DiscoveryResult:
        started_ms = int(time.time() * 1000)
        service = self._control_service_factory(client_id="unitlab-discovery-worker")
        try:
            logger.info("External IED MMS discovery configuring target | workspace=%s endpoint=%s", request.workspace_id, request.endpoint)
            service.configure_target(
                Iec61850ClientTargetRequest(
                    mode="external-mms",
                    host=request.ip,
                    port=request.port,
                )
            )
            logger.info("External IED MMS discovery connecting | workspace=%s endpoint=%s", request.workspace_id, request.endpoint)
            service.connect_ied()
            logger.info("External IED MMS discovery reading model | workspace=%s endpoint=%s", request.workspace_id, request.endpoint)
            snapshot = service.discover_ied()
            discovery = snapshot.last_discovery if isinstance(snapshot.last_discovery, dict) else {}
            model = build_discovery_model(discovery)
            summary = build_discovery_summary(model, duration_ms=max(0, int(time.time() * 1000) - started_ms))
            model_fingerprint = compute_model_fingerprint(model)
            now_ms = int(time.time() * 1000)
            metadata = DiscoveryCacheMetadata(
                discovery_version="unitlab.external-ied.discovery.v1",
                device_identity=_device_identity(discovery),
                vendor=_first_non_empty(discovery.get("vendor"), discovery.get("manufacturer")),
                model=_first_non_empty(discovery.get("model"), discovery.get("modelName")),
                config_rev=_first_non_empty(discovery.get("configRev"), discovery.get("config_rev")),
                last_discovery_at_ms=now_ms,
                last_successful_discovery_at_ms=now_ms,
                last_failed_discovery_at_ms=None,
                last_error=None,
                discovery_duration_ms=summary.duration_ms,
                model_fingerprint=model_fingerprint,
                planning_fingerprint=request.planning_fingerprint,
            )
            return DiscoveryResult(request=request, metadata=metadata, summary=summary, model=model)
        finally:
            close_ied = getattr(service, "close_ied", None)
            if callable(close_ied):
                close_ied()


def build_discovery_model(discovery: dict[str, Any]) -> dict[str, Any]:
    datasets = []
    for dataset in discovery.get("dataSets") or discovery.get("datasets") or []:
        if not isinstance(dataset, dict):
            continue
        reference = _first_non_empty(dataset.get("reference"), dataset.get("ref"), dataset.get("id"))
        if reference is None:
            continue
        members = []
        for member in dataset.get("members") or []:
            if isinstance(member, dict):
                member_ref = _first_non_empty(member.get("mmsReference"), member.get("reference"), member.get("ref"))
                if member_ref:
                    members.append({"reference": member_ref, "fc": _first_non_empty(member.get("fc"), member.get("functionalConstraint"))})
        datasets.append({
            "reference": reference,
            "members": sorted(
                {json.dumps(member, sort_keys=True, separators=(",", ":")) for member in members},
            ),
        })

    rcbs = []
    for rcb in discovery.get("reportControls") or discovery.get("report_controls") or []:
        if not isinstance(rcb, dict):
            continue
        reference = _first_non_empty(rcb.get("id"), rcb.get("reference"), rcb.get("rcbRef"))
        name = _first_non_empty(rcb.get("name"), rcb.get("reportControlName"), rcb.get("report_control_name")) or reference
        dataset_ref = _first_non_empty(rcb.get("dataSetRef"), rcb.get("data_set_ref"), rcb.get("dataset"))
        if reference is None or dataset_ref is None:
            continue
        rcbs.append({
            "reference": reference,
            "name": name,
            "dataset_reference": dataset_ref,
            "kind": _first_non_empty(rcb.get("kind"), rcb.get("reportKind")) or "unknown",
            "conf_rev": _first_non_empty(rcb.get("confRev"), rcb.get("conf_rev")),
        })

    fcdas = {}
    for dataset in datasets:
        normalized_members = []
        for raw_member in dataset["members"]:
            member = json.loads(raw_member)
            member_ref = member["reference"]
            normalized_members.append(member_ref)
            fcdas.setdefault(member_ref, {"reference": member_ref, "fc": member.get("fc") or _functional_constraint(member_ref)})
        dataset["members"] = normalized_members

    return {
        "schema": "unitlab.external-ied.model.v1",
        "ied": _device_identity(discovery),
        "datasets": sorted(datasets, key=lambda item: item["reference"]),
        "rcbs": sorted(rcbs, key=lambda item: item["reference"]),
        "fcdas": sorted(fcdas.values(), key=lambda item: item["reference"]),
    }


def build_discovery_summary(model: dict[str, Any], *, duration_ms: int) -> DiscoverySummary:
    datasets = model.get("datasets") if isinstance(model.get("datasets"), list) else []
    rcbs = model.get("rcbs") if isinstance(model.get("rcbs"), list) else []
    fcdas = model.get("fcdas") if isinstance(model.get("fcdas"), list) else []
    return DiscoverySummary(
        logical_devices=1 if model.get("ied") else 0,
        logical_nodes=_logical_node_count(fcdas),
        datasets=len(datasets),
        dataset_members=sum(len(dataset.get("members") or []) for dataset in datasets if isinstance(dataset, dict)),
        report_controls=len(rcbs),
        signals=len(fcdas),
        duration_ms=duration_ms,
    )


def compute_model_fingerprint(model: dict[str, Any]) -> str:
    raw = json.dumps(model, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(raw).hexdigest()


def _device_identity(discovery: dict[str, Any]) -> str | None:
    endpoint = discovery.get("endpoint") if isinstance(discovery.get("endpoint"), dict) else {}
    return _first_non_empty(
        discovery.get("iedName"),
        discovery.get("ied_name"),
        endpoint.get("iedName"),
        endpoint.get("ied_name"),
    )


def _logical_node_count(fcdas: list[Any]) -> int:
    nodes = set()
    for fcda in fcdas:
        if not isinstance(fcda, dict):
            continue
        reference = str(fcda.get("reference") or "")
        if "/" in reference:
            rest = reference.split("/", 1)[1]
            nodes.add(rest.split(".", 1)[0])
    return len(nodes)


def _functional_constraint(reference: str) -> str | None:
    if reference.endswith("[ST]"):
        return "ST"
    if reference.endswith("[MX]"):
        return "MX"
    parts = reference.split("$")
    if len(parts) >= 3 and len(parts[1]) == 2:
        return parts[1].upper()
    return None


def _first_non_empty(*values: Any) -> str | None:
    for value in values:
        if isinstance(value, str) and value.strip():
            return value.strip()
    return None
