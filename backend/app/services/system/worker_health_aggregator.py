from __future__ import annotations

import asyncio

from app.core.config import get_settings
from app.core.events.ws_event_publisher import WsEventPublisher
from app.core.logger import get_logger
from app.schemas.ws.events import SystemHealthChangedEvent
from app.services.worker_health import (
    SystemHealthDiff,
    SystemHealthSnapshot,
    build_system_snapshot,
    collect_worker_health,
    diff_snapshots,
    get_cached_system_snapshot,
    snapshot_to_dict,
    store_system_snapshot,
)

settings = get_settings()
logger = get_logger("worker.health.aggregator")


async def _compute_snapshot() -> SystemHealthSnapshot:
    workers = await collect_worker_health()
    snapshot = build_system_snapshot(workers)
    await store_system_snapshot(snapshot)
    return snapshot


async def _publish_change(diff: SystemHealthDiff, snapshot: SystemHealthSnapshot) -> None:
    logger.debug(
        "System health changed: %s → %s (%s)",
        diff.previous_status,
        diff.current_status,
        diff.changed_workers,
    )
    event = SystemHealthChangedEvent(
        previous_status=diff.previous_status,
        current_status=diff.current_status,
        changed_at=snapshot.checked_at,
        issues=snapshot.issues,
        diff={"workers": diff.changed_workers},
        snapshot=snapshot_to_dict(snapshot),
    )
    await WsEventPublisher.publish(event)


async def run_worker_health_aggregator() -> None:
    interval = max(1, settings.worker_health_interval)
    previous_snapshot = await get_cached_system_snapshot()

    if previous_snapshot is None:
        previous_snapshot = await _compute_snapshot()
        diff = diff_snapshots(None, previous_snapshot)
        await _publish_change(diff, previous_snapshot)

    while True:
        try:
            snapshot = await _compute_snapshot()
            diff = diff_snapshots(previous_snapshot, snapshot)
            if diff.has_changes:
                await _publish_change(diff, snapshot)
            previous_snapshot = snapshot
        except asyncio.CancelledError:
            raise
        except Exception as exc:  # noqa: BLE001
            logger.error("💥 Worker health aggregation failed: %s", exc)
        await asyncio.sleep(interval)
