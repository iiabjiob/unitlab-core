from __future__ import annotations

import hashlib
import json
import time
from collections.abc import Callable
from dataclasses import dataclass
from typing import Protocol, cast

from app.core.logger import get_logger
from app.services.external_ied_discovery_scheduler import (
    DiscoveryCacheMetadata,
    EXTERNAL_IED_DISCOVERY_VERSION,
    ExternalIedDiscoveryRequest,
)
from app.services.iec61850.client_control import (
    Iec61850ClientControlService,
    Iec61850ClientTargetRequest,
)

logger = get_logger("external_ied_discovery")
JsonObject = dict[str, object]


def _json_object_list(value: object) -> list[JsonObject]:
    if not isinstance(value, list):
        return []
    return [cast(JsonObject, item) for item in cast(list[object], value) if isinstance(item, dict)]


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
    model: JsonObject


class ExternalIedDiscoveryEngine(Protocol):
    def discover(self, request: ExternalIedDiscoveryRequest) -> DiscoveryResult: ...


class MmsExternalIedDiscoveryEngine:
    def __init__(
        self,
        *,
        control_service_factory: Callable[..., Iec61850ClientControlService] = Iec61850ClientControlService,
    ) -> None:
        self._control_service_factory: Callable[..., Iec61850ClientControlService] = control_service_factory

    def discover(self, request: ExternalIedDiscoveryRequest) -> DiscoveryResult:
        started_ms = int(time.time() * 1000)
        service = self._control_service_factory(client_id="unitlab-discovery-worker")
        try:
            logger.info("External IED MMS discovery configuring target | workspace=%s endpoint=%s", request.workspace_id, request.endpoint)
            _ = service.configure_target(
                Iec61850ClientTargetRequest(
                    mode="external-mms",
                    host=request.ip,
                    port=request.port,
                )
            )
            logger.info("External IED MMS discovery connecting | workspace=%s endpoint=%s", request.workspace_id, request.endpoint)
            _ = service.connect_ied()
            logger.info("External IED MMS discovery reading model | workspace=%s endpoint=%s", request.workspace_id, request.endpoint)
            snapshot = service.discover_ied()
            discovery = snapshot.last_discovery if isinstance(snapshot.last_discovery, dict) else {}
            model = build_discovery_model(discovery)
            summary = build_discovery_summary(model, duration_ms=max(0, int(time.time() * 1000) - started_ms))
            model_fingerprint = compute_model_fingerprint(model)
            now_ms = int(time.time() * 1000)
            metadata = DiscoveryCacheMetadata(
                discovery_version=EXTERNAL_IED_DISCOVERY_VERSION,
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
                _ = close_ied()


def build_discovery_model(discovery: JsonObject) -> JsonObject:
    datasets: list[JsonObject] = []
    for dataset in _json_object_list(discovery.get("dataSets") or discovery.get("datasets")):
        reference = _first_non_empty(dataset.get("reference"), dataset.get("ref"), dataset.get("id"))
        if reference is None:
            continue
        members: list[JsonObject] = []
        for member in _json_object_list(dataset.get("members")):
            member_ref = _first_non_empty(member.get("mmsReference"), member.get("reference"), member.get("ref"))
            if member_ref:
                members.append(cast(JsonObject, {"reference": member_ref, "fc": _first_non_empty(member.get("fc"), member.get("functionalConstraint"))}))
        datasets.append({
            "reference": reference,
            "members": sorted(
                {json.dumps(member, sort_keys=True, separators=(",", ":")) for member in members},
            ),
        })

    rcbs: list[JsonObject] = []
    for rcb in _json_object_list(discovery.get("reportControls") or discovery.get("report_controls")):
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

    fcdas: dict[str, JsonObject] = {}
    for dataset in datasets:
        normalized_members: list[str] = []
        for raw_member in cast(list[str], dataset["members"]):
            member = cast(JsonObject, json.loads(raw_member))
            member_ref = str(member["reference"])
            normalized_members.append(member_ref)
            _ = fcdas.setdefault(member_ref, cast(JsonObject, {"reference": member_ref, "fc": member.get("fc") or _functional_constraint(member_ref)}))
        dataset["members"] = normalized_members

    return {
        "schema": "unitlab.external-ied.model.v1",
        "ied": _device_identity(discovery),
        "datasets": sorted(datasets, key=lambda item: str(item["reference"])),
        "rcbs": sorted(rcbs, key=lambda item: str(item["reference"])),
        "fcdas": sorted(fcdas.values(), key=lambda item: str(item["reference"])),
        "diagnostics": _diagnostics(discovery),
    }


def build_discovery_summary(model: JsonObject, *, duration_ms: int) -> DiscoverySummary:
    datasets = _model_list(model, "datasets")
    rcbs = _model_list(model, "rcbs")
    fcdas = _model_list(model, "fcdas")
    return DiscoverySummary(
        logical_devices=1 if model.get("ied") else 0,
        logical_nodes=_logical_node_count(fcdas),
        datasets=len(datasets),
        dataset_members=sum(
            len(cast(list[object], dataset.get("members")))
            if isinstance(dataset.get("members"), list)
            else 0
            for dataset in datasets
        ),
        report_controls=len(rcbs),
        signals=len(fcdas),
        duration_ms=duration_ms,
    )


def compute_model_fingerprint(model: JsonObject) -> str:
    fingerprint_model = {key: value for key, value in model.items() if key != "diagnostics"}
    raw = json.dumps(fingerprint_model, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(raw).hexdigest()


def _device_identity(discovery: JsonObject) -> str | None:
    endpoint_value = discovery.get("endpoint")
    endpoint = cast(JsonObject, endpoint_value) if isinstance(endpoint_value, dict) else {}
    return _first_non_empty(
        discovery.get("iedName"),
        discovery.get("ied_name"),
        endpoint.get("iedName"),
        endpoint.get("ied_name"),
    )


def _model_list(model: JsonObject, key: str) -> list[JsonObject]:
    value = model.get(key)
    return _json_object_list(value)


def _logical_node_count(fcdas: list[JsonObject]) -> int:
    nodes: set[str] = set()
    for fcda in fcdas:
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


def _first_non_empty(*values: object) -> str | None:
    for value in values:
        if isinstance(value, str) and value.strip():
            return value.strip()
    return None


def _diagnostics(discovery: JsonObject) -> list[str]:
    values = discovery.get("diagnostics")
    if not isinstance(values, list):
        return []
    result: list[str] = []
    for value in cast(list[object], values):
        text = str(value or "").strip()
        if text and text not in result:
            result.append(text)
    return result
