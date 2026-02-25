from __future__ import annotations

import asyncio
import json
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Iterable, Literal

from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager

settings = get_settings()
logger = get_logger("worker.health")

WorkerStatus = Literal["online", "degraded", "offline", "unknown"]
SNAPSHOT_KEY = "system:health:snapshot"


@dataclass(frozen=True)
class WorkerDefinition:
    name: str
    display_name: str
    impact: str


WORKER_DEFINITIONS: tuple[WorkerDefinition, ...] = (
    WorkerDefinition(
        name="mqtt_ingress",
        display_name="MQTT Ingress",
        impact="MQTT ingress is down — inbound MQTT traffic is not processed.",
    ),
    WorkerDefinition(
        name="inbound_processor",
        display_name="MQTT Stream Processor",
        impact="Redis inbound stream is not consumed — device data is stale.",
    ),
    WorkerDefinition(
        name="mqtt_outbound",
        display_name="MQTT Outbound",
        impact="MQTT outbound is down — commands are not sent to devices.",
    ),
    WorkerDefinition(
        name="sequence_runner",
        display_name="Sequence Runner",
        impact="Sequences are not running — queued commands are not executed.",
    ),
    WorkerDefinition(
        name="device_offline",
        display_name="Offline Checker",
        impact="Device statuses become stale — online/offline state is not recalculated.",
    ),
    WorkerDefinition(
        name="signal_allocation_runner",
        display_name="Signal Allocation Runner",
        impact="Async signal allocation jobs are not processed.",
    ),
    WorkerDefinition(
        name="signal_test_run_runner",
        display_name="Signal Test Run Runner",
        impact="Signal test runs cannot progress or resume.",
    ),
)

WORKER_REGISTRY = {definition.name: definition for definition in WORKER_DEFINITIONS}


@dataclass
class WorkerHealth:
    name: str
    display_name: str
    status: WorkerStatus
    last_seen_at: datetime | None
    impact: str
    detail: str | None = None


@dataclass
class SystemHealthSnapshot:
    status: Literal["online", "degraded", "offline"]
    checked_at: datetime
    issues: list[str]
    workers: list[WorkerHealth]


@dataclass(frozen=True)
class SystemHealthDiff:
    previous_status: Literal["online", "degraded", "offline"] | None
    current_status: Literal["online", "degraded", "offline"]
    changed_workers: list[str]

    @property
    def has_changes(self) -> bool:
        return bool(self.changed_workers) or (self.previous_status != self.current_status)


def _worker_key(worker_name: str) -> str:
    return f"worker:health:{worker_name}"


def _normalize_status(value: str | None) -> WorkerStatus:
    if value in {"online", "degraded", "offline", "unknown"}:
        return value  # type: ignore[return-value]
    return "unknown"


async def write_worker_status(worker_name: str, *, status: WorkerStatus = "online", detail: str | None = None) -> None:
    redis = RedisManager.get_instance()
    payload = {
        "status": status,
        "detail": detail,
        "updated_at": datetime.now(timezone.utc).isoformat(),
    }
    await redis.set(_worker_key(worker_name), json.dumps(payload), ex=settings.worker_health_ttl)


async def clear_worker_status(worker_name: str) -> None:
    redis = RedisManager.get_instance()
    try:
        await redis.delete(_worker_key(worker_name))
    except Exception as exc:  # noqa: BLE001
        logger.warning("Failed to clear worker status for %s: %s", worker_name, exc)


async def _worker_heartbeat_loop(worker_name: str, *, status: WorkerStatus, detail: str | None) -> None:
    interval = max(1, min(settings.worker_health_interval, max(1, settings.worker_health_ttl // 2)))
    while True:
        try:
            await write_worker_status(worker_name, status=status, detail=detail)
        except Exception as exc:  # noqa: BLE001
            logger.error("Failed to update heartbeat for %s: %s", worker_name, exc)
        await asyncio.sleep(interval)


def start_worker_heartbeat(worker_name: str, *, status: WorkerStatus = "online", detail: str | None = None) -> asyncio.Task[None]:
    return asyncio.create_task(_worker_heartbeat_loop(worker_name, status=status, detail=detail))


async def collect_worker_health() -> list[WorkerHealth]:
    redis = RedisManager.get_instance()
    now = datetime.now(timezone.utc)
    ttl = settings.worker_health_ttl
    statuses: list[WorkerHealth] = []

    for definition in WORKER_DEFINITIONS:
        raw = await redis.get(_worker_key(definition.name))
        status: WorkerStatus = "offline"
        detail: str | None = None
        last_seen: datetime | None = None

        if raw:
            try:
                data = json.loads(raw)
                status = _normalize_status(data.get("status"))
                detail = data.get("detail")
                updated_at = data.get("updated_at")
                if updated_at:
                    last_seen = datetime.fromisoformat(updated_at)
            except Exception as exc:  # noqa: BLE001
                logger.warning("Failed to parse health payload for %s: %s", definition.name, exc)
                status = "unknown"
            else:
                if last_seen and (now - last_seen).total_seconds() > ttl:
                    status = "offline"
                    detail = detail or "heartbeat timeout"
        statuses.append(
            WorkerHealth(
                name=definition.name,
                display_name=definition.display_name,
                status=status,
                last_seen_at=last_seen,
                impact=definition.impact,
                detail=detail,
            )
        )

    return statuses


def compute_system_status(workers: Iterable[WorkerHealth]) -> Literal["online", "degraded", "offline"]:
    workers = list(workers)
    if not workers:
        return "online"

    non_online = [worker for worker in workers if worker.status != "online"]
    if len(non_online) == len(workers):
        return "offline"
    if non_online:
        return "degraded"
    return "online"


def _issue_text(worker: WorkerHealth) -> str:
    if worker.detail:
        return f"{worker.display_name}: {worker.impact} ({worker.detail})"
    return f"{worker.display_name}: {worker.impact}"


def build_system_snapshot(workers: list[WorkerHealth]) -> SystemHealthSnapshot:
    status = compute_system_status(workers)
    issues = [_issue_text(worker) for worker in workers if worker.status != "online"]
    return SystemHealthSnapshot(
        status=status,
        checked_at=datetime.now(timezone.utc),
        issues=issues,
        workers=workers,
    )


def _worker_to_dict(worker: WorkerHealth) -> dict[str, object]:
    return {
        "name": worker.name,
        "display_name": worker.display_name,
        "status": worker.status,
        "last_seen_at": worker.last_seen_at.isoformat() if worker.last_seen_at else None,
        "impact": worker.impact,
        "detail": worker.detail,
    }


def _worker_from_dict(payload: dict[str, object]) -> WorkerHealth:
    last_seen = payload.get("last_seen_at")
    last_seen_dt = datetime.fromisoformat(last_seen) if isinstance(last_seen, str) else None
    detail_raw = payload.get("detail")
    detail = detail_raw if isinstance(detail_raw, str) else None
    return WorkerHealth(
        name=str(payload.get("name")),
        display_name=str(payload.get("display_name")),
        status=_normalize_status(str(payload.get("status"))),
        last_seen_at=last_seen_dt,
        impact=str(payload.get("impact")),
        detail=detail,
    )


def snapshot_to_dict(snapshot: SystemHealthSnapshot) -> dict[str, object]:
    return {
        "status": snapshot.status,
        "checked_at": snapshot.checked_at.isoformat(),
        "issues": list(snapshot.issues),
        "workers": [_worker_to_dict(worker) for worker in snapshot.workers],
    }


def snapshot_from_dict(payload: dict[str, object]) -> SystemHealthSnapshot:
    checked_at_raw = payload.get("checked_at")
    checked_at = datetime.fromisoformat(checked_at_raw) if isinstance(checked_at_raw, str) else datetime.now(timezone.utc)
    workers_payload = payload.get("workers")
    workers = [_worker_from_dict(item) for item in workers_payload or []]
    issues_payload = payload.get("issues")
    if isinstance(issues_payload, list):
        issues = [str(item) for item in issues_payload]
    else:
        issues = []
    status_raw = payload.get("status")
    status = status_raw if isinstance(status_raw, str) and status_raw in {"online", "degraded", "offline"} else "degraded"
    return SystemHealthSnapshot(
        status=status,
        checked_at=checked_at,
        issues=issues,
        workers=workers,
    )


async def store_system_snapshot(snapshot: SystemHealthSnapshot) -> None:
    redis = RedisManager.get_instance()
    await redis.set(SNAPSHOT_KEY, json.dumps(snapshot_to_dict(snapshot)))


async def get_cached_system_snapshot() -> SystemHealthSnapshot | None:
    redis = RedisManager.get_instance()
    raw = await redis.get(SNAPSHOT_KEY)
    if not raw:
        return None
    return snapshot_from_dict(json.loads(raw))


def diff_snapshots(
    previous: SystemHealthSnapshot | None,
    current: SystemHealthSnapshot,
) -> SystemHealthDiff:
    prev_map = {worker.name: (worker.status, worker.detail) for worker in (previous.workers if previous else [])}
    changed_workers: list[str] = []
    for worker in current.workers:
        prev_state = prev_map.get(worker.name)
        current_state = (worker.status, worker.detail)
        if prev_state != current_state:
            changed_workers.append(worker.name)
    return SystemHealthDiff(
        previous_status=previous.status if previous else None,
        current_status=current.status,
        changed_workers=changed_workers,
    )


__all__ = [
    "WorkerHealth",
    "SystemHealthSnapshot",
    "SystemHealthDiff",
    "collect_worker_health",
    "compute_system_status",
    "build_system_snapshot",
    "snapshot_to_dict",
    "snapshot_from_dict",
    "diff_snapshots",
    "get_cached_system_snapshot",
    "store_system_snapshot",
    "start_worker_heartbeat",
    "clear_worker_status",
    "write_worker_status",
]
