from __future__ import annotations

import json

import pytest

from app.schemas.signal_sheet_schema import SignalAllocationRowSchema
from app.services import external_ied_planning as planning


class _FakeRedis:
    def __init__(self) -> None:
        self.hashes: dict[str, dict[str, str]] = {}
        self.streams: dict[str, list[dict[str, str]]] = {}

    async def hget(self, key: str, field: str) -> str | None:
        return self.hashes.get(key, {}).get(field)

    async def hgetall(self, key: str) -> dict[str, str]:
        return dict(self.hashes.get(key, {}))

    async def hset(self, key: str, field: str, value: str) -> None:
        self.hashes.setdefault(key, {})[field] = value

    async def hdel(self, key: str, field: str) -> None:
        self.hashes.setdefault(key, {}).pop(field, None)

    async def smembers(self, _key: str) -> set[str]:
        return {"5"}

    async def xadd(self, stream: str, fields: dict, **_kwargs) -> str:
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


async def _rows(_db, _request):
    return [_row(101, "IEDLD0/GGIO1.ST.stVal")]


async def _incomplete_domain_rows(_db, _request):
    return [_row(101, "KINTE15BCU01CTRL2/DARGAPC6/Ind20/stVal[ST]")]


class _FakeSignalSheetRepository:
    def __init__(self, _db) -> None:
        self.list_all_rows_calls = 0
        self.list_by_ids_calls = 0

    async def list_allocation_rows(self, workspace_id: int):
        self.list_all_rows_calls += 1
        assert workspace_id == 5
        return [
            _row(101, "KINTE15BCU01CTRL2/DARGAPC6/Ind20/stVal[ST]"),
            _row(102, "KINTE15BCU01CTRL1/GGIO1/stVal[ST]", host="172.16.40.128:12448"),
        ]

    async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids):
        self.list_by_ids_calls += 1
        assert workspace_id == 5
        return [_row(signal_id, "KINTE15BCU01CTRL2/DARGAPC6/Ind20/stVal[ST]") for signal_id in signal_ids]


@pytest.mark.anyio
async def test_external_ied_planning_builds_signal_coverage(monkeypatch) -> None:
    redis = _FakeRedis()
    published = _Published()
    monkeypatch.setattr(planning.RedisManager, "get_instance", lambda: redis)
    monkeypatch.setattr(planning.WsEventPublisher, "publish", published.publish)
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

    plan = await planning.execute_external_ied_planning_request(request, db=None)  # type: ignore[arg-type]

    assert plan is not None
    assert plan.signals[0].status == "planned"
    endpoint_payload = json.loads(redis.hashes["external_ied:workspace:5:planning_state"]["172.16.40.128:12447"])
    assert endpoint_payload["state"] == "Ready"
    signal_payload = json.loads(redis.hashes["external_ied:workspace:5:planning_signal"]["101"])
    assert signal_payload["status"] == "matched"
    assert signal_payload["rcb_name"] == "brcb01"
    assert published.events[-1].event == "external_ied_planning_changed"
    assert published.events[-1].signal_results[0].status == "matched"


@pytest.mark.anyio
async def test_external_ied_planning_waits_for_discovery_cache(monkeypatch) -> None:
    redis = _FakeRedis()
    published = _Published()
    monkeypatch.setattr(planning.RedisManager, "get_instance", lambda: redis)
    monkeypatch.setattr(planning.WsEventPublisher, "publish", published.publish)
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

    plan = await planning.execute_external_ied_planning_request(request, db=None)  # type: ignore[arg-type]

    assert plan is None
    endpoint_payload = json.loads(redis.hashes["external_ied:workspace:5:planning_state"]["172.16.40.128:12447"])
    assert endpoint_payload["state"] == "WaitingForDiscovery"
    assert "external_ied:workspace:5:planning_signal" not in redis.hashes
    assert published.events[-1].endpoint.state == "WaitingForDiscovery"


@pytest.mark.anyio
async def test_external_ied_planning_marks_incomplete_discovery_as_stale(monkeypatch) -> None:
    redis = _FakeRedis()
    published = _Published()
    monkeypatch.setattr(planning.RedisManager, "get_instance", lambda: redis)
    monkeypatch.setattr(planning.WsEventPublisher, "publish", published.publish)
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

    plan = await planning.execute_external_ied_planning_request(request, db=None)  # type: ignore[arg-type]

    assert plan is not None
    endpoint_payload = json.loads(redis.hashes["external_ied:workspace:5:planning_state"]["172.16.40.128:12447"])
    assert endpoint_payload["state"] == "Stale"
    signal_payload = json.loads(redis.hashes["external_ied:workspace:5:planning_signal"]["101"])
    assert signal_payload["status"] == "stale"
    assert signal_payload["reason"] == "discovery cache has no dataset members for IED domain"
    assert published.events[-1].signal_results[0].status == "stale"


@pytest.mark.anyio
async def test_external_ied_planning_loads_endpoint_rows_when_request_has_no_signal_ids(monkeypatch) -> None:
    repository = _FakeSignalSheetRepository(None)
    monkeypatch.setattr(planning, "SignalSheetRepository", lambda _db: repository)
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

    rows = await planning._load_mapped_rows(None, request)  # type: ignore[arg-type]

    assert repository.list_all_rows_calls == 1
    assert repository.list_by_ids_calls == 0
    assert [row.signal_id for row in rows] == [101]
