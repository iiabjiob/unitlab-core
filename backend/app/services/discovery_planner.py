from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass, field
from typing import Literal, Sequence

from app.services.external_ied_discovery_scheduler import DiscoveryCacheMetadata


PlanAction = Literal["NoAction", "PlanningOnly", "DiscoveryThenPlanning"]
PlanSignalStatus = Literal["planned", "unresolved"]
IEC61850_FUNCTIONAL_CONSTRAINTS = {
    "st", "mx", "sp", "sv", "cf", "dc", "sg", "se", "sr", "or", "bl", "ex", "co", "fc",
}
PREFER_BUFFERED_RCB = True


@dataclass(frozen=True, slots=True)
class DiscoveryPlanSignalInput:
    signal_id: int
    address: str
    ied_identity: str | None = None
    source_row_id: str | None = None


@dataclass(frozen=True, slots=True)
class DiscoveryCacheFcda:
    reference: str
    fc: str | None = None


@dataclass(frozen=True, slots=True)
class DiscoveryCacheDataset:
    reference: str
    members: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True)
class DiscoveryCacheRcb:
    reference: str
    name: str
    dataset_reference: str
    kind: Literal["buffered", "unbuffered", "unknown"] = "unknown"
    conf_rev: str | None = None


@dataclass(frozen=True, slots=True)
class DiscoveryCacheIed:
    metadata: DiscoveryCacheMetadata
    fcdas: tuple[DiscoveryCacheFcda, ...] = ()
    datasets: tuple[DiscoveryCacheDataset, ...] = ()
    rcbs: tuple[DiscoveryCacheRcb, ...] = ()

    @property
    def identity(self) -> str:
        return self.metadata.device_identity or "unknown"


@dataclass(frozen=True, slots=True)
class DiscoveryCache:
    ieds: tuple[DiscoveryCacheIed, ...] = ()
    model_stale: bool = False


@dataclass(frozen=True, slots=True)
class VerificationPlanSignal:
    signal_id: int
    address: str
    status: PlanSignalStatus
    ied_identity: str | None = None
    fcda_reference: str | None = None
    functional_constraint: str | None = None
    dataset_reference: str | None = None
    rcb_reference: str | None = None
    rcb_name: str | None = None
    reason: str | None = None
    reused_from_previous_plan: bool = False


@dataclass(frozen=True, slots=True)
class VerificationPlanDependencyGraph:
    nodes: tuple[dict[str, str], ...] = ()
    edges: tuple[dict[str, str], ...] = ()


@dataclass(frozen=True, slots=True)
class VerificationPlan:
    plan_id: str
    planning_fingerprint: str
    model_fingerprints: dict[str, str]
    signals: tuple[VerificationPlanSignal, ...]
    dependency_graph: VerificationPlanDependencyGraph
    diagnostics: tuple[str, ...] = ()
    action: PlanAction = "PlanningOnly"
    reused_signal_ids: tuple[int, ...] = ()
    recomputed_signal_ids: tuple[int, ...] = ()
    discovery_required: bool = False


@dataclass(frozen=True, slots=True)
class DiscoveryPlannerChange:
    action: PlanAction
    reason: str
    signal_list_changed: bool = False
    model_changed: bool = False


class DiscoveryPlanner:
    def evaluate_change(
        self,
        *,
        signal_list: Sequence[DiscoveryPlanSignalInput],
        discovery_cache: DiscoveryCache,
        previous_plan: VerificationPlan | None = None,
    ) -> DiscoveryPlannerChange:
        if discovery_cache.model_stale or not discovery_cache.ieds:
            return DiscoveryPlannerChange(action="DiscoveryThenPlanning", reason="discovery cache missing or stale")
        model_fingerprints = _model_fingerprints(discovery_cache)
        if any(not value for value in model_fingerprints.values()):
            return DiscoveryPlannerChange(action="DiscoveryThenPlanning", reason="IED model fingerprint missing")
        planning_fingerprint = _planning_fingerprint(signal_list)
        if previous_plan is None:
            return DiscoveryPlannerChange(action="PlanningOnly", reason="no previous plan")
        if previous_plan.model_fingerprints != model_fingerprints:
            return DiscoveryPlannerChange(action="PlanningOnly", reason="IED model cache changed", model_changed=True)
        if previous_plan.planning_fingerprint != planning_fingerprint:
            return DiscoveryPlannerChange(action="PlanningOnly", reason="signal list changed", signal_list_changed=True)
        return DiscoveryPlannerChange(action="NoAction", reason="plan inputs unchanged")

    def build_plan(
        self,
        *,
        signal_list: Sequence[DiscoveryPlanSignalInput],
        discovery_cache: DiscoveryCache,
        previous_plan: VerificationPlan | None = None,
    ) -> VerificationPlan:
        change = self.evaluate_change(
            signal_list=signal_list,
            discovery_cache=discovery_cache,
            previous_plan=previous_plan,
        )
        planning_fingerprint = _planning_fingerprint(signal_list)
        model_fingerprints = _model_fingerprints(discovery_cache)
        if change.action == "DiscoveryThenPlanning":
            return VerificationPlan(
                plan_id=_plan_id(planning_fingerprint, model_fingerprints),
                planning_fingerprint=planning_fingerprint,
                model_fingerprints=model_fingerprints,
                signals=(),
                dependency_graph=VerificationPlanDependencyGraph(),
                diagnostics=(change.reason,),
                action=change.action,
                discovery_required=True,
            )

        previous_by_key = {
            _signal_key(signal.signal_id, signal.address, signal.ied_identity): signal
            for signal in previous_plan.signals
        } if previous_plan is not None and previous_plan.model_fingerprints == model_fingerprints else {}

        planned: list[VerificationPlanSignal] = []
        reused_signal_ids: list[int] = []
        recomputed_signal_ids: list[int] = []
        graph = _GraphBuilder()
        for signal in signal_list:
            key = _signal_key(signal.signal_id, signal.address, signal.ied_identity)
            previous = previous_by_key.get(key)
            if previous is not None:
                planned.append(_copy_reused(previous))
                reused_signal_ids.append(signal.signal_id)
                _add_signal_to_graph(graph, planned[-1])
                continue

            planned_signal = _plan_signal(signal, discovery_cache)
            planned.append(planned_signal)
            recomputed_signal_ids.append(signal.signal_id)
            _add_signal_to_graph(graph, planned_signal)

        diagnostics = [change.reason]
        unresolved = sum(1 for signal in planned if signal.status == "unresolved")
        if unresolved:
            diagnostics.append(f"{unresolved} signals unresolved")

        return VerificationPlan(
            plan_id=_plan_id(planning_fingerprint, model_fingerprints),
            planning_fingerprint=planning_fingerprint,
            model_fingerprints=model_fingerprints,
            signals=tuple(planned),
            dependency_graph=graph.build(),
            diagnostics=tuple(diagnostics),
            action=change.action,
            reused_signal_ids=tuple(reused_signal_ids),
            recomputed_signal_ids=tuple(recomputed_signal_ids),
            discovery_required=False,
        )


def _plan_signal(signal: DiscoveryPlanSignalInput, discovery_cache: DiscoveryCache) -> VerificationPlanSignal:
    ied = _select_ied(signal, discovery_cache)
    if ied is None:
        return _unresolved(signal, "IED not found in discovery cache")
    fcda = _lookup_fcda(signal.address, ied)
    if fcda is None:
        return _unresolved(signal, "FCDA not found in discovery cache", ied_identity=ied.identity)
    dataset = _lookup_dataset(fcda, ied)
    if dataset is None:
        return _unresolved(signal, "dataset not found for FCDA", ied_identity=ied.identity, fcda=fcda)
    rcb = _select_rcb(dataset, ied)
    if rcb is None:
        return _unresolved(signal, "RCB not found for dataset", ied_identity=ied.identity, fcda=fcda, dataset=dataset)
    return VerificationPlanSignal(
        signal_id=signal.signal_id,
        address=signal.address,
        status="planned",
        ied_identity=ied.identity,
        fcda_reference=fcda.reference,
        functional_constraint=fcda.fc,
        dataset_reference=dataset.reference,
        rcb_reference=rcb.reference,
        rcb_name=rcb.name,
    )


def _select_ied(signal: DiscoveryPlanSignalInput, discovery_cache: DiscoveryCache) -> DiscoveryCacheIed | None:
    if signal.ied_identity:
        desired = signal.ied_identity.strip().lower()
        for ied in discovery_cache.ieds:
            if ied.identity.lower() == desired:
                return ied
        return None
    for ied in discovery_cache.ieds:
        if _lookup_fcda(signal.address, ied) is not None:
            return ied
    return None


def _lookup_fcda(address: str, ied: DiscoveryCacheIed) -> DiscoveryCacheFcda | None:
    desired = _canonical_address(address)
    for fcda in ied.fcdas:
        candidate = _canonical_address(fcda.reference)
        if desired == candidate or desired.endswith("." + candidate) or candidate.endswith("." + desired):
            return fcda
    return None


def _lookup_dataset(fcda: DiscoveryCacheFcda, ied: DiscoveryCacheIed) -> DiscoveryCacheDataset | None:
    fcda_ref = _canonical_address(fcda.reference)
    for dataset in ied.datasets:
        if any(_canonical_address(member) == fcda_ref for member in dataset.members):
            return dataset
    return None


def _select_rcb(dataset: DiscoveryCacheDataset, ied: DiscoveryCacheIed) -> DiscoveryCacheRcb | None:
    candidates = [rcb for rcb in ied.rcbs if _same_reference(rcb.dataset_reference, dataset.reference)]
    if not candidates:
        return None
    return sorted(candidates, key=_rcb_selection_key)[0]


def _rcb_selection_key(rcb: DiscoveryCacheRcb) -> tuple[int, str, str]:
    buffered_rank = 0 if PREFER_BUFFERED_RCB and rcb.kind == "buffered" else 1
    return buffered_rank, rcb.name, rcb.reference


def _unresolved(
    signal: DiscoveryPlanSignalInput,
    reason: str,
    *,
    ied_identity: str | None = None,
    fcda: DiscoveryCacheFcda | None = None,
    dataset: DiscoveryCacheDataset | None = None,
) -> VerificationPlanSignal:
    return VerificationPlanSignal(
        signal_id=signal.signal_id,
        address=signal.address,
        status="unresolved",
        ied_identity=ied_identity,
        fcda_reference=fcda.reference if fcda else None,
        functional_constraint=fcda.fc if fcda else None,
        dataset_reference=dataset.reference if dataset else None,
        reason=reason,
    )


def _copy_reused(signal: VerificationPlanSignal) -> VerificationPlanSignal:
    return VerificationPlanSignal(
        signal_id=signal.signal_id,
        address=signal.address,
        status=signal.status,
        ied_identity=signal.ied_identity,
        fcda_reference=signal.fcda_reference,
        functional_constraint=signal.functional_constraint,
        dataset_reference=signal.dataset_reference,
        rcb_reference=signal.rcb_reference,
        rcb_name=signal.rcb_name,
        reason=signal.reason,
        reused_from_previous_plan=True,
    )


@dataclass
class _GraphBuilder:
    nodes: dict[str, dict[str, str]] = field(default_factory=dict)
    edges: set[tuple[str, str]] = field(default_factory=set)

    def add_node(self, node_id: str, kind: str, label: str) -> None:
        self.nodes.setdefault(node_id, {"id": node_id, "kind": kind, "label": label})

    def add_edge(self, source: str, target: str) -> None:
        self.edges.add((source, target))

    def build(self) -> VerificationPlanDependencyGraph:
        return VerificationPlanDependencyGraph(
            nodes=tuple(sorted(self.nodes.values(), key=lambda item: item["id"])),
            edges=tuple({"source": source, "target": target} for source, target in sorted(self.edges)),
        )


def _add_signal_to_graph(graph: _GraphBuilder, signal: VerificationPlanSignal) -> None:
    signal_node = f"signal:{signal.signal_id}"
    graph.add_node(signal_node, "Signal", str(signal.signal_id))
    previous = signal_node
    for kind, value in (
        ("FCDA", signal.fcda_reference),
        ("Dataset", signal.dataset_reference),
        ("RCB", signal.rcb_reference),
        ("IED", signal.ied_identity),
    ):
        if not value:
            continue
        node = f"{kind.lower()}:{value}"
        graph.add_node(node, kind, value)
        graph.add_edge(previous, node)
        previous = node


def _planning_fingerprint(signal_list: Sequence[DiscoveryPlanSignalInput]) -> str:
    payload = [
        {
            "signal_id": signal.signal_id,
            "address": signal.address.strip(),
            "ied_identity": (signal.ied_identity or "").strip(),
            "source_row_id": signal.source_row_id or "",
        }
        for signal in signal_list
    ]
    raw = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(raw).hexdigest()


def _model_fingerprints(discovery_cache: DiscoveryCache) -> dict[str, str]:
    return {
        ied.identity: ied.metadata.model_fingerprint or ""
        for ied in discovery_cache.ieds
    }


def _plan_id(planning_fingerprint: str, model_fingerprints: dict[str, str]) -> str:
    payload = {"planning": planning_fingerprint, "models": model_fingerprints}
    raw = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(raw).hexdigest()[:16]


def _signal_key(signal_id: int, address: str, ied_identity: str | None) -> str:
    return f"{signal_id}:{(ied_identity or '').strip()}:{address.strip()}"


def _same_reference(left: str, right: str) -> bool:
    return _canonical_address(left) == _canonical_address(right)


def _canonical_address(value: str) -> str:
    text = str(value or "").strip()
    if "!" in text:
        text = text.split("!", 1)[1]
    text = re.sub(r"\[[A-Za-z0-9]+\]$", "", text)
    text = text.replace("/", ".").replace("$", ".")
    parts = [part for part in text.lower().split(".") if part]
    if len(parts) > 2:
        parts = [part for index, part in enumerate(parts) if index <= 1 or part not in IEC61850_FUNCTIONAL_CONSTRAINTS]
    return ".".join(parts)
