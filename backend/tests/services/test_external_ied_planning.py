from __future__ import annotations

import json
from typing import cast

import pytest
from sqlalchemy.ext.asyncio import AsyncSession

from app.schemas.signal_sheet_schema import SignalAllocationRowSchema
from app.schemas.ws.events import ExternalIedPlanningChangedEvent
from app.services import external_ied_planning as planning


class _FakeRedis:
    def __init__(self) -> None:
        self.hashes: dict[str, dict[str, str]] = {}
        self.streams: dict[str, list[dict[str, object]]] = {}

    async def hget(self, key: str, field: str) -> str | None:
        return self.hashes.get(key, {}).get(field)

    async def hgetall(self, key: str) -> dict[str, str]:
        return dict(self.hashes.get(key, {}))

    async def hset(self, key: str, field: str, value: str) -> None:
        self.hashes.setdefault(key, {})[field] = value

    async def hdel(self, key: str, field: str) -> None:
        _ = self.hashes.setdefault(key, {}).pop(field, None)

    async def smembers(self, _key: str) -> set[str]:
        return {"5"}

    async def xadd(self, stream: str, fields: dict[str, object], **_kwargs: object) -> str:
        self.streams.setdefault(stream, []).append(fields)
        return f"{len(self.streams[stream])}-0"


class _Published:
    def __init__(self) -> None:
        self.events: list[object] = []

    async def publish(self, event: object) -> None:
        self.events.append(event)


def _row(signal_id: int, address: str, host: str = "172.16.40.128:12447") -> SignalAllocationRowSchema:
    return SignalAllocationRowSchema(
        row_id=f"signal-{signal_id}",
        signal_id=signal_id,
        signal_key=f"S{signal_id}",
        signal_name=f"Signal {signal_id}",
        signal_direction="DI",
        signal_metadata={
            "verification": {
                "enabled": True,
                "transport_host": host,
                "iec61850_address": address,
            },
            "row": {
                "transport_host": host,
                "iec61850_address": address,
            },
        },
    )


async def _rows(_db: AsyncSession, _request: planning.ExternalIedPlanningRequest) -> list[SignalAllocationRowSchema]:
    return [_row(101, "IEDLD0/GGIO1.ST.stVal")]


async def _incomplete_domain_rows(
    _db: AsyncSession, _request: planning.ExternalIedPlanningRequest
) -> list[SignalAllocationRowSchema]:
    return [_row(101, "KINTE15BCU01CTRL2/DARGAPC6/Ind20/stVal[ST]")]


class _FakeSignalSheetRepository:
    def __init__(self, _db: AsyncSession) -> None:
        self.list_all_rows_calls: int = 0
        self.list_by_ids_calls: int = 0

    async def list_allocation_rows(self, workspace_id: int) -> list[SignalAllocationRowSchema]:
        self.list_all_rows_calls += 1
        assert workspace_id == 5
        return [
            _row(101, "KINTE15BCU01CTRL2/DARGAPC6/Ind20/stVal[ST]"),
            _row(102, "KINTE15BCU01CTRL1/GGIO1/stVal[ST]", host="172.16.40.128:12448"),
        ]

    async def list_allocation_rows_by_signal_ids(
        self, workspace_id: int, signal_ids: tuple[int, ...]
    ) -> list[SignalAllocationRowSchema]:
        self.list_by_ids_calls += 1
        assert workspace_id == 5
        return [_row(signal_id, "KINTE15BCU01CTRL2/DARGAPC6/Ind20/stVal[ST]") for signal_id in signal_ids]


@pytest.mark.anyio
async def test_external_ied_planning_builds_signal_coverage(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    published = _Published()
    monkeypatch.setattr(planning.RedisManager, "get_instance", lambda: redis)  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(planning.WsEventPublisher, "publish", published.publish)  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(planning, "_load_mapped_rows", _rows)
    await redis.hset(
        "external_ied:workspace:5:discovery_cache",
        "172.16.40.128:12447",
        json.dumps({"device_identity": "IED-A", "model_fingerprint": "model-a"}),
    )
    await redis.hset(
        "external_ied:workspace:5:discovery_model",
        "172.16.40.128:12447",
        json.dumps({
            "fcdas": [{"reference": "IEDLD0/GGIO1.ST.stVal", "fc": "ST"}],
            "datasets": [{"reference": "IEDLD0/LLN0.ds1", "members": ["IEDLD0/GGIO1.ST.stVal"]}],
            "rcbs": [{"reference": "IEDLD0/LLN0.BR.brcb01", "name": "brcb01", "kind": "buffered", "dataset_reference": "IEDLD0/LLN0.ds1"}],
        }),
    )
    request = planning.ExternalIedPlanningRequest(
        request_id="req-1",
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        ip="172.16.40.128",
        port=12447,
        signal_ids=(101,),
        reason="discovery_completed",
        requested_at_ms=1000,
    )

    plan = await planning.execute_external_ied_planning_request(
        request, db=cast(AsyncSession, cast(object, None))
    )

    assert plan is not None
    assert plan.signals[0].status == "planned"
    endpoint_payload = cast(dict[str, object], json.loads(redis.hashes["external_ied:workspace:5:planning_state"]["172.16.40.128:12447"]))
    assert endpoint_payload["state"] == "Ready"
    signal_payload = cast(dict[str, object], json.loads(redis.hashes["external_ied:workspace:5:planning_signal"]["101"]))
    assert signal_payload["status"] == "matched"
    assert signal_payload["rcb_name"] == "brcb01"
    event = cast(ExternalIedPlanningChangedEvent, published.events[-1])
    assert event.event == "external_ied_planning_changed"
    assert event.signal_results[0].status == "matched"


@pytest.mark.anyio
async def test_external_ied_planning_waits_for_discovery_cache(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    published = _Published()
    monkeypatch.setattr(planning.RedisManager, "get_instance", lambda: redis)  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(planning.WsEventPublisher, "publish", published.publish)  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(planning, "_load_mapped_rows", _rows)
    request = planning.ExternalIedPlanningRequest(
        request_id="req-1",
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        ip="172.16.40.128",
        port=12447,
        signal_ids=(101,),
        reason="mapping_changed",
        requested_at_ms=1000,
    )

    plan = await planning.execute_external_ied_planning_request(
        request, db=cast(AsyncSession, cast(object, None))
    )

    assert plan is None
    endpoint_payload = cast(dict[str, object], json.loads(redis.hashes["external_ied:workspace:5:planning_state"]["172.16.40.128:12447"]))
    assert endpoint_payload["state"] == "WaitingForDiscovery"
    assert "external_ied:workspace:5:planning_signal" not in redis.hashes
    event = cast(ExternalIedPlanningChangedEvent, published.events[-1])
    assert event.endpoint.state == "WaitingForDiscovery"


@pytest.mark.anyio
async def test_external_ied_planning_marks_incomplete_discovery_as_stale(monkeypatch: pytest.MonkeyPatch) -> None:
    redis = _FakeRedis()
    published = _Published()
    monkeypatch.setattr(planning.RedisManager, "get_instance", lambda: redis)  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(planning.WsEventPublisher, "publish", published.publish)  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(planning, "_load_mapped_rows", _incomplete_domain_rows)
    await redis.hset(
        "external_ied:workspace:5:discovery_cache",
        "172.16.40.128:12447",
        json.dumps({"device_identity": "IED-A", "model_fingerprint": "model-a"}),
    )
    await redis.hset(
        "external_ied:workspace:5:discovery_model",
        "172.16.40.128:12447",
        json.dumps({
            "fcdas": [],
            "datasets": [{"reference": "KINTE15BCU01CTRL2/LLN0$LLN0BRptStDs", "members": []}],
            "rcbs": [
                {
                    "reference": "KINTE15BCU01CTRL2:LLN0$BR$brcbST01",
                    "name": "brcbST",
                    "kind": "buffered",
                    "dataset_reference": "KINTE15BCU01CTRL2/LLN0$LLN0BRptStDs",
                },
            ],
        }),
    )
    request = planning.ExternalIedPlanningRequest(
        request_id="req-1",
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        ip="172.16.40.128",
        port=12447,
        signal_ids=(101,),
        reason="discovery_completed",
        requested_at_ms=1000,
    )

    plan = await planning.execute_external_ied_planning_request(
        request, db=cast(AsyncSession, cast(object, None))
    )

    assert plan is not None
    endpoint_payload = cast(dict[str, object], json.loads(redis.hashes["external_ied:workspace:5:planning_state"]["172.16.40.128:12447"]))
    assert endpoint_payload["state"] == "Stale"
    signal_payload = cast(dict[str, object], json.loads(redis.hashes["external_ied:workspace:5:planning_signal"]["101"]))
    assert signal_payload["status"] == "stale"
    assert signal_payload["reason"] == "discovery cache has no dataset members for IED domain"
    event = cast(ExternalIedPlanningChangedEvent, published.events[-1])
    assert event.signal_results[0].status == "stale"


@pytest.mark.anyio
async def test_external_ied_planning_loads_endpoint_rows_when_request_has_no_signal_ids(monkeypatch: pytest.MonkeyPatch) -> None:
    repository = _FakeSignalSheetRepository(cast(AsyncSession, cast(object, None)))
    def _repository_factory(_db: AsyncSession) -> _FakeSignalSheetRepository:
        return repository

    monkeypatch.setattr(planning, "SignalSheetRepository", _repository_factory)
    request = planning.ExternalIedPlanningRequest(
        request_id="req-1",
        workspace_id=5,
        endpoint="172.16.40.128:12447",
        ip="172.16.40.128",
        port=12447,
        signal_ids=(),
        reason="discovery_completed",
        requested_at_ms=1000,
    )

    rows = await planning._load_mapped_rows(  # pyright: ignore[reportPrivateUsage]
        cast(AsyncSession, cast(object, None)), request
    )

    assert repository.list_all_rows_calls == 1
    assert repository.list_by_ids_calls == 0
    assert [row.signal_id for row in rows] == [101]
