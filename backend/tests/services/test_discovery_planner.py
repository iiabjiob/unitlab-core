from __future__ import annotations

from app.services.discovery_planner import (
    DiscoveryCache,
    DiscoveryCacheDataset,
    DiscoveryCacheFcda,
    DiscoveryCacheIed,
    DiscoveryCacheRcb,
    DiscoveryPlanSignalInput,
    DiscoveryPlanner,
)
from app.services.external_ied_discovery_scheduler import DiscoveryCacheMetadata


def _cache(*, model_fingerprint: str = "model-v1", model_stale: bool = False) -> DiscoveryCache:
    return DiscoveryCache(
        model_stale=model_stale,
        ieds=(
            DiscoveryCacheIed(
                metadata=DiscoveryCacheMetadata(
                    discovery_version="unitlab-discovery.v1",
                    device_identity="IED-A",
                    vendor="UnitLab",
                    model="VirtualIED",
                    config_rev="7",
                    last_discovery_at_ms=100,
                    last_successful_discovery_at_ms=100,
                    discovery_duration_ms=35,
                    model_fingerprint=model_fingerprint,
                    planning_fingerprint="plan-from-engine",
                ),
                fcdas=(
                    DiscoveryCacheFcda(reference="IED-ACTRL/LLN0.Pos.stVal", fc="ST"),
                    DiscoveryCacheFcda(reference="IED-ACTRL/LLN0.Amp.mag.f", fc="MX"),
                ),
                datasets=(
                    DiscoveryCacheDataset(
                        reference="IED-ACTRL/LLN0.dsST",
                        members=("IED-ACTRL/LLN0.Pos.stVal",),
                    ),
                    DiscoveryCacheDataset(
                        reference="IED-ACTRL/LLN0.dsMX",
                        members=("IED-ACTRL/LLN0.Amp.mag.f",),
                    ),
                ),
                rcbs=(
                    DiscoveryCacheRcb(
                        reference="IED-A/LLN0.brST",
                        name="brST",
                        dataset_reference="IED-ACTRL/LLN0.dsST",
                        kind="buffered",
                        conf_rev="7",
                    ),
                    DiscoveryCacheRcb(
                        reference="IED-A/LLN0.urST",
                        name="urST",
                        dataset_reference="IED-ACTRL/LLN0.dsST",
                        kind="unbuffered",
                        conf_rev="7",
                    ),
                ),
            ),
        ),
    )


def _iec_address_cache() -> DiscoveryCache:
    return DiscoveryCache(
        ieds=(
            DiscoveryCacheIed(
                metadata=DiscoveryCacheMetadata(
                    device_identity="IED-A",
                    model_fingerprint="model-v1",
                ),
                fcdas=(
                    DiscoveryCacheFcda(reference="LD0/LLN0.Pos.stVal", fc="ST"),
                    DiscoveryCacheFcda(reference="LD0/LLN0.Amp.mag.f", fc="MX"),
                ),
                datasets=(
                    DiscoveryCacheDataset(
                        reference="LD0/LLN0.dsST",
                        members=("LD0/LLN0$ST$Pos$stVal",),
                    ),
                    DiscoveryCacheDataset(
                        reference="LD0/LLN0.dsMX",
                        members=("LD0/LLN0.Amp.mag.f",),
                    ),
                ),
                rcbs=(
                    DiscoveryCacheRcb(
                        reference="LD0/LLN0.urFast",
                        name="aaa-ur",
                        dataset_reference="LD0/LLN0.dsST",
                        kind="unbuffered",
                    ),
                    DiscoveryCacheRcb(
                        reference="LD0/LLN0.brStable",
                        name="zzz-br",
                        dataset_reference="LD0/LLN0.dsST",
                        kind="buffered",
                    ),
                    DiscoveryCacheRcb(
                        reference="LD0/LLN0.brMX",
                        name="brMX",
                        dataset_reference="LD0/LLN0.dsMX",
                        kind="buffered",
                    ),
                ),
            ),
        ),
    )


def _do_level_member_cache() -> DiscoveryCache:
    return DiscoveryCache(
        ieds=(
            DiscoveryCacheIed(
                metadata=DiscoveryCacheMetadata(
                    device_identity="IED-A",
                    model_fingerprint="model-v1",
                ),
                fcdas=(
                    DiscoveryCacheFcda(reference="IED-ACTRL/XCBR1$ST$Pos", fc="ST"),
                    DiscoveryCacheFcda(reference="IED-ACTRL/MMXU1$MX$A", fc="MX"),
                ),
                datasets=(
                    DiscoveryCacheDataset(
                        reference="IED-ACTRL/LLN0.dsST",
                        members=("IED-ACTRL/XCBR1$ST$Pos",),
                    ),
                    DiscoveryCacheDataset(
                        reference="IED-ACTRL/LLN0.dsMX",
                        members=("IED-ACTRL/MMXU1$MX$A",),
                    ),
                ),
                rcbs=(
                    DiscoveryCacheRcb(
                        reference="IED-ACTRL/LLN0.brST",
                        name="brST",
                        dataset_reference="IED-ACTRL/LLN0.dsST",
                        kind="buffered",
                    ),
                    DiscoveryCacheRcb(
                        reference="IED-ACTRL/LLN0.brMX",
                        name="brMX",
                        dataset_reference="IED-ACTRL/LLN0.dsMX",
                        kind="buffered",
                    ),
                ),
            ),
        ),
    )


def test_discovery_planner_binds_signal_to_fcda_dataset_rcb_ied_without_mms() -> None:
    plan = DiscoveryPlanner().build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="IED-ACTRL/LLN0.Pos.stVal[ST]", ied_identity="IED-A"),
        ),
        discovery_cache=_cache(),
    )

    assert plan.discovery_required is False
    assert plan.signals[0].status == "planned"
    assert plan.signals[0].fcda_reference == "IED-ACTRL/LLN0.Pos.stVal"
    assert plan.signals[0].functional_constraint == "ST"
    assert plan.signals[0].dataset_reference == "IED-ACTRL/LLN0.dsST"
    assert plan.signals[0].rcb_reference == "IED-A/LLN0.brST"
    assert plan.signals[0].ied_identity == "IED-A"
    assert ("signal:1", "fcda:IED-ACTRL/LLN0.Pos.stVal") in {
        (edge["source"], edge["target"]) for edge in plan.dependency_graph.edges
    }


def test_signal_list_change_reuses_discovery_cache_and_recomputes_planning_only() -> None:
    planner = DiscoveryPlanner()
    previous = planner.build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="IED-ACTRL/LLN0.Pos.stVal[ST]", ied_identity="IED-A"),
        ),
        discovery_cache=_cache(),
    )

    next_plan = planner.build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="IED-ACTRL/LLN0.Pos.stVal[ST]", ied_identity="IED-A"),
            DiscoveryPlanSignalInput(signal_id=2, address="IED-ACTRL/LLN0.Amp.mag.f[MX]", ied_identity="IED-A"),
        ),
        discovery_cache=_cache(),
        previous_plan=previous,
    )

    assert next_plan.action == "PlanningOnly"
    assert next_plan.discovery_required is False
    assert next_plan.reused_signal_ids == (1,)
    assert next_plan.recomputed_signal_ids == (2,)
    assert next_plan.signals[0].reused_from_previous_plan is True


def test_stale_or_missing_ied_model_requests_discovery_then_planning() -> None:
    planner = DiscoveryPlanner()
    previous = planner.build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="IED-ACTRL/LLN0.Pos.stVal[ST]", ied_identity="IED-A"),
        ),
        discovery_cache=_cache(model_fingerprint="model-v1"),
    )

    stale_change = planner.evaluate_change(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="IED-ACTRL/LLN0.Pos.stVal[ST]", ied_identity="IED-A"),
        ),
        discovery_cache=_cache(model_fingerprint="model-v1", model_stale=True),
        previous_plan=previous,
    )
    missing_fingerprint = planner.evaluate_change(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="IED-ACTRL/LLN0.Pos.stVal[ST]", ied_identity="IED-A"),
        ),
        discovery_cache=_cache(model_fingerprint=""),
        previous_plan=previous,
    )

    assert stale_change.action == "DiscoveryThenPlanning"
    assert missing_fingerprint.action == "DiscoveryThenPlanning"


def test_changed_cached_model_triggers_planning_only_not_rediscovery() -> None:
    planner = DiscoveryPlanner()
    previous = planner.build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="IED-ACTRL/LLN0.Pos.stVal[ST]", ied_identity="IED-A"),
        ),
        discovery_cache=_cache(model_fingerprint="model-v1"),
    )

    changed_model = planner.evaluate_change(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="IED-ACTRL/LLN0.Pos.stVal[ST]", ied_identity="IED-A"),
        ),
        discovery_cache=_cache(model_fingerprint="model-v2"),
        previous_plan=previous,
    )

    assert changed_model.action == "PlanningOnly"
    assert changed_model.model_changed is True


def test_planner_matches_common_iec61850_address_formats_and_fc_suffixes() -> None:
    plan = DiscoveryPlanner().build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="LD0/LLN0$ST$Pos$stVal", ied_identity="IED-A"),
            DiscoveryPlanSignalInput(signal_id=2, address="LD0/LLN0.Pos.stVal", ied_identity="IED-A"),
            DiscoveryPlanSignalInput(signal_id=3, address="IED-A!LD0/LLN0$ST$Pos$stVal", ied_identity="IED-A"),
            DiscoveryPlanSignalInput(signal_id=4, address="LD0/LLN0.Pos.stVal[ST]", ied_identity="IED-A"),
            DiscoveryPlanSignalInput(signal_id=5, address="LD0/LLN0$MX$Amp$mag$f", ied_identity="IED-A"),
            DiscoveryPlanSignalInput(signal_id=6, address="LD0/LLN0.Amp.mag.f[MX]", ied_identity="IED-A"),
        ),
        discovery_cache=_iec_address_cache(),
    )

    assert [signal.status for signal in plan.signals] == ["planned"] * 6
    assert {signal.fcda_reference for signal in plan.signals[:4]} == {"LD0/LLN0.Pos.stVal"}
    assert {signal.fcda_reference for signal in plan.signals[4:]} == {"LD0/LLN0.Amp.mag.f"}


def test_planner_prefers_buffered_rcb_explicitly() -> None:
    plan = DiscoveryPlanner().build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="LD0/LLN0.Pos.stVal", ied_identity="IED-A"),
        ),
        discovery_cache=_iec_address_cache(),
    )

    assert plan.signals[0].rcb_reference == "LD0/LLN0.brStable"
    assert plan.signals[0].rcb_name == "zzz-br"


def test_planner_treats_dataset_do_member_as_covering_signal_leaf_address() -> None:
    plan = DiscoveryPlanner().build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="IED-ACTRL/XCBR1.Pos.stVal[ST]", ied_identity="IED-A"),
            DiscoveryPlanSignalInput(signal_id=2, address="IED-ACTRL/MMXU1.A.phsA.cVal.mag.f[MX]", ied_identity="IED-A"),
        ),
        discovery_cache=_do_level_member_cache(),
    )

    assert [signal.status for signal in plan.signals] == ["planned", "planned"]
    assert plan.signals[0].fcda_reference == "IED-ACTRL/XCBR1$ST$Pos"
    assert plan.signals[0].dataset_reference == "IED-ACTRL/LLN0.dsST"
    assert plan.signals[1].fcda_reference == "IED-ACTRL/MMXU1$MX$A"
    assert plan.signals[1].dataset_reference == "IED-ACTRL/LLN0.dsMX"


def test_planner_matches_signal_address_with_concatenated_ied_prefix() -> None:
    plan = DiscoveryPlanner().build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="IED-ACTRL/XCBR1.Pos.stVal[ST]", ied_identity="IED-A"),
        ),
        discovery_cache=DiscoveryCache(
            ieds=(
                DiscoveryCacheIed(
                    metadata=DiscoveryCacheMetadata(device_identity="IED-A", model_fingerprint="model-v1"),
                    fcdas=(DiscoveryCacheFcda(reference="CTRL/XCBR1$ST$Pos", fc="ST"),),
                    datasets=(DiscoveryCacheDataset(reference="CTRL/LLN0.dsST", members=("CTRL/XCBR1$ST$Pos",)),),
                    rcbs=(
                        DiscoveryCacheRcb(
                            reference="CTRL/LLN0.brST",
                            name="brST",
                            dataset_reference="CTRL/LLN0.dsST",
                            kind="buffered",
                        ),
                    ),
                ),
            ),
        ),
    )

    assert plan.signals[0].status == "planned"
    assert plan.signals[0].fcda_reference == "CTRL/XCBR1$ST$Pos"


def test_planner_matches_signal_list_slash_leaf_format_to_mms_dataset_member() -> None:
    plan = DiscoveryPlanner().build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="KINTE15BCU01CTRL1/DisconCSWI1/Pos/stVal[ST]"),
        ),
        discovery_cache=DiscoveryCache(
            ieds=(
                DiscoveryCacheIed(
                    metadata=DiscoveryCacheMetadata(device_identity="KINTE15BCU01", model_fingerprint="model-v1"),
                    fcdas=(
                        DiscoveryCacheFcda(reference="KINTE15BCU01CTRL1/DisconCSWI1$OR$Pos", fc="OR"),
                        DiscoveryCacheFcda(reference="KINTE15BCU01CTRL1/DisconCSWI1$ST$Pos", fc="ST"),
                    ),
                    datasets=(
                        DiscoveryCacheDataset(
                            reference="KINTE15BCU01CTRL1/LLN0$LLN0BRptStDs",
                            members=(
                                "KINTE15BCU01CTRL1/DisconCSWI1$OR$Pos",
                                "KINTE15BCU01CTRL1/DisconCSWI1$ST$Pos",
                            ),
                        ),
                    ),
                    rcbs=(
                        DiscoveryCacheRcb(
                            reference="KINTE15BCU01CTRL1:LLN0$BR$brcbST01",
                            name="brcbST",
                            dataset_reference="KINTE15BCU01CTRL1/LLN0$LLN0BRptStDs",
                            kind="buffered",
                        ),
                    ),
                ),
            ),
        ),
    )

    assert plan.signals[0].status == "planned"
    assert plan.signals[0].fcda_reference == "KINTE15BCU01CTRL1/DisconCSWI1$ST$Pos"


def test_planner_matches_control_oper_signal_to_or_dataset_member() -> None:
    plan = DiscoveryPlanner().build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="KINTE15BCU01CTRL1/CBCSWI1/Pos/Oper.ctlVal[CO]"),
        ),
        discovery_cache=DiscoveryCache(
            ieds=(
                DiscoveryCacheIed(
                    metadata=DiscoveryCacheMetadata(device_identity="KINTE15BCU01", model_fingerprint="model-v1"),
                    fcdas=(
                        DiscoveryCacheFcda(reference="KINTE15BCU01CTRL1/CBCSWI1$OR$Pos", fc="OR"),
                        DiscoveryCacheFcda(reference="KINTE15BCU01CTRL1/CBCSWI1$ST$Pos", fc="ST"),
                    ),
                    datasets=(
                        DiscoveryCacheDataset(
                            reference="KINTE15BCU01CTRL1/LLN0$LLN0BRptOrDs",
                            members=("KINTE15BCU01CTRL1/CBCSWI1$OR$Pos",),
                        ),
                    ),
                    rcbs=(
                        DiscoveryCacheRcb(
                            reference="KINTE15BCU01CTRL1:LLN0$BR$brcbOR01",
                            name="brcbOR",
                            dataset_reference="KINTE15BCU01CTRL1/LLN0$LLN0BRptOrDs",
                            kind="buffered",
                        ),
                    ),
                ),
            ),
        ),
    )

    assert plan.signals[0].status == "planned"
    assert plan.signals[0].fcda_reference == "KINTE15BCU01CTRL1/CBCSWI1$OR$Pos"
    assert plan.signals[0].functional_constraint == "OR"
    assert plan.signals[0].rcb_name == "brcbOR"


def test_planner_prefers_exact_co_member_before_or_control_fallback() -> None:
    plan = DiscoveryPlanner().build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="KINTE15BCU01CTRL1/CBCSWI1/Pos/Oper.ctlVal[CO]"),
        ),
        discovery_cache=DiscoveryCache(
            ieds=(
                DiscoveryCacheIed(
                    metadata=DiscoveryCacheMetadata(device_identity="KINTE15BCU01", model_fingerprint="model-v1"),
                    fcdas=(
                        DiscoveryCacheFcda(reference="KINTE15BCU01CTRL1/CBCSWI1$OR$Pos", fc="OR"),
                        DiscoveryCacheFcda(reference="KINTE15BCU01CTRL1/CBCSWI1$CO$Pos$Oper$ctlVal", fc="CO"),
                    ),
                    datasets=(
                        DiscoveryCacheDataset(
                            reference="KINTE15BCU01CTRL1/LLN0$LLN0BRptCoDs",
                            members=("KINTE15BCU01CTRL1/CBCSWI1$CO$Pos$Oper$ctlVal",),
                        ),
                    ),
                    rcbs=(
                        DiscoveryCacheRcb(
                            reference="KINTE15BCU01CTRL1:LLN0$BR$brcbCO01",
                            name="brcbCO",
                            dataset_reference="KINTE15BCU01CTRL1/LLN0$LLN0BRptCoDs",
                            kind="buffered",
                        ),
                    ),
                ),
            ),
        ),
    )

    assert plan.signals[0].status == "planned"
    assert plan.signals[0].fcda_reference == "KINTE15BCU01CTRL1/CBCSWI1$CO$Pos$Oper$ctlVal"


def test_planner_marks_domain_with_empty_dataset_members_as_incomplete_discovery() -> None:
    plan = DiscoveryPlanner().build_plan(
        signal_list=(
            DiscoveryPlanSignalInput(signal_id=1, address="KINTE15BCU01CTRL2/DARGAPC6/Ind20/stVal[ST]"),
        ),
        discovery_cache=DiscoveryCache(
            ieds=(
                DiscoveryCacheIed(
                    metadata=DiscoveryCacheMetadata(device_identity="KINTE15BCU01", model_fingerprint="model-v1"),
                    datasets=(DiscoveryCacheDataset(reference="KINTE15BCU01CTRL2/LLN0$LLN0BRptStDs", members=()),),
                    rcbs=(
                        DiscoveryCacheRcb(
                            reference="KINTE15BCU01CTRL2:LLN0$BR$brcbST01",
                            name="brcbST",
                            dataset_reference="KINTE15BCU01CTRL2/LLN0$LLN0BRptStDs",
                            kind="buffered",
                        ),
                    ),
                ),
            ),
        ),
    )

    assert plan.signals[0].status == "unresolved"
    assert plan.signals[0].reason == "discovery cache has no dataset members for IED domain"
