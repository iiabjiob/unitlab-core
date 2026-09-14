from __future__ import annotations

from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import exists, select, update

from app.models.hardware_command import HardwareCommandIntent


async def has_hardware_recovery_required(
    db: AsyncSession,
    *,
    workspace_id: int,
    channel_id: int,
) -> bool:
    result = await db.execute(
        select(
            exists().where(
                HardwareCommandIntent.workspace_id == workspace_id,
                HardwareCommandIntent.channel_id == channel_id,
                HardwareCommandIntent.status.in_(("unknown", "recovery_required")),
            )
        )
    )
    return bool(result.scalar())


async def reconcile_unfinished_hardware_command_intents(
    db: AsyncSession,
    *,
    job_id: str,
    attempt_id: str | None = None,
) -> int:
    """Make commands from an interrupted attempt explicit before replay is failed."""
    filters = [
        HardwareCommandIntent.job_id == job_id,
        HardwareCommandIntent.status.in_(("created", "queued")),
    ]
    if attempt_id:
        filters.append(HardwareCommandIntent.attempt_id == attempt_id)
    result = await db.execute(select(HardwareCommandIntent).where(*filters))
    intents = list(result.scalars().all())
    for intent in intents:
        intent.status = "recovery_required" if intent.action == "restore" else "unknown"
    if intents:
        await db.flush()
    return len(intents)


async def record_hardware_command_intent(
    db: AsyncSession,
    *,
    command_id: str,
    workspace_id: int,
    job_id: str | None,
    attempt_id: str | None,
    owner_kind: str,
    owner_id: str,
    device_id: int | None,
    channel_id: int,
    unit_id: str,
    action: str,
    payload: dict,
    fencing_epoch: int | None,
) -> HardwareCommandIntent:
    intent = HardwareCommandIntent(
        command_id=command_id,
        workspace_id=workspace_id,
        job_id=job_id,
        attempt_id=attempt_id,
        owner_kind=owner_kind,
        owner_id=owner_id,
        device_id=device_id,
        channel_id=channel_id,
        unit_id=unit_id,
        action=action,
        payload=dict(payload),
        fencing_epoch=fencing_epoch,
        status="created",
    )
    db.add(intent)
    await db.flush()
    return intent


async def mark_hardware_command_intent_queued(
    db: AsyncSession,
    *,
    command_id: str,
) -> None:
    await db.execute(
        update(HardwareCommandIntent)
        .where(HardwareCommandIntent.command_id == command_id)
        .values(status="queued")
    )
    await db.flush()


async def mark_hardware_command_intent_delivery_failure(
    db: AsyncSession,
    *,
    command_id: str,
    status: str,
) -> None:
    await db.execute(
        update(HardwareCommandIntent)
        .where(HardwareCommandIntent.command_id == command_id)
        .values(status=status)
    )
    await db.flush()
