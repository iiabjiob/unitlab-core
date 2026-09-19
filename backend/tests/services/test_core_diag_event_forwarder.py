from collections.abc import Mapping
from typing import cast

from sqlalchemy.ext.asyncio import AsyncSession

from app.models.core_diagnostics import CoreDiagnosticsAcknowledgement
from app.services.core_diag_event_forwarder import _incident_id  # pyright: ignore[reportPrivateUsage]
from app.services.core_diagnostics_incident import (
    DiagnosticsRedisClient,
    acknowledge_incident,
    is_incident_acknowledged,
    record_incident_acknowledgement,
)


class _Redis:
    def __init__(self) -> None:
        self.values: dict[str, str] = {}
        self.events: list[dict[str, str]] = []

    async def set(self, name: str, value: object, **_kwargs: object) -> None:
        self.values[name] = str(value)

    async def get(self, name: str) -> object:
        return self.values.get(name)

    async def xadd(self, name: str, fields: Mapping[str, object], **_kwargs: object) -> str:
        self.events.append({"stream": name, **{key: str(value) for key, value in fields.items()}})
        return "1-0"


def _snapshot(*, cpu: float = 86.0, memory: float = 96.0, disk: float = 96.0) -> dict[str, object]:
    return {
        "mode": "degraded",
        "cpu": {"temperature_c": cpu},
        "memory": {"used_percent": memory},
        "disk_root": {"used_percent": disk},
        "services": [{"name": "docker", "active": True}],
    }


def test_incident_id_ignores_numeric_measurement_drift() -> None:
    assert _incident_id(_snapshot(cpu=86.0, memory=96.0, disk=96.0)) == _incident_id(
        _snapshot(cpu=91.5, memory=98.0, disk=99.0)
    )


def test_incident_id_changes_for_different_required_service_failure() -> None:
    docker_down = _snapshot()
    docker_down["services"] = [{"name": "docker", "active": False}]
    network_manager_down = _snapshot()
    network_manager_down["services"] = [{"name": "NetworkManager", "active": False}]

    assert _incident_id(docker_down) != _incident_id(network_manager_down)


def test_healthy_snapshot_has_no_incident_id() -> None:
    assert _incident_id(
        {
            "mode": "ok",
            "cpu": {"temperature_c": 40.0},
            "memory": {"used_percent": 30.0},
            "disk_root": {"used_percent": 40.0},
            "services": [{"name": "docker", "active": True}],
        }
    ) is None


def test_incident_acknowledgement_is_ttl_backed_and_scoped() -> None:
    import asyncio

    redis = _Redis()
    typed_redis = cast(DiagnosticsRedisClient, cast(object, redis))
    assert asyncio.run(acknowledge_incident(typed_redis, hostname="rpi-1", incident_id="core-diag:abc")) is True
    assert asyncio.run(is_incident_acknowledged(typed_redis, hostname="rpi-1", incident_id="core-diag:abc")) is True
    assert asyncio.run(is_incident_acknowledged(typed_redis, hostname="rpi-2", incident_id="core-diag:abc")) is False
    assert next(iter(redis.values)) == "core:diagnostics:ack:rpi-1:core-diag:abc"
    assert redis.events[0]["stream"] == "core:diagnostics:ack-events"
    assert "core_diagnostics_acknowledged" in redis.events[0]["event"]


def test_incident_acknowledgement_is_persisted_as_append_only_record() -> None:
    import asyncio

    class Db:
        def __init__(self) -> None:
            self.records: list[CoreDiagnosticsAcknowledgement] = []

        def add(self, record: CoreDiagnosticsAcknowledgement) -> None:
            self.records.append(record)

        async def commit(self) -> None:
            return None

    db = Db()
    asyncio.run(record_incident_acknowledgement(cast(AsyncSession, cast(object, db)), hostname="rpi-1", incident_id="core-diag:abc"))
    assert db.records[0].hostname == "rpi-1"
    assert db.records[0].incident_id == "core-diag:abc"
    assert db.records[0].actor == "websocket-anonymous"
