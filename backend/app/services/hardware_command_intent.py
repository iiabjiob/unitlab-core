from __future__ import annotations

from datetime import datetime, timedelta, timezone

from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import and_, exists, or_, select, update

from app.models.hardware_command import HardwareCommandIntent, HardwareCommandIntentChannel

RECOVERY_REQUIRED_ACTIONS = frozenset({"restore", "do_pulse"})
HARDWARE_RETRY_DELAY_SECONDS = 10


def hardware_retry_cutoff() -> datetime:
    return datetime.now(timezone.utc) - timedelta(seconds=HARDWARE_RETRY_DELAY_SECONDS)


async def hardware_command_publish_allowed(db: AsyncSession, *, command_id: str) -> bool:
    """Never replay a timed-out, expired or completed tracked command."""
    result = await db.execute(
        select(HardwareCommandIntent).where(HardwareCommandIntent.command_id == command_id)
    )
    intent = result.scalar_one_or_none()
    if intent is None:
        return True
    created_at = intent.created_at
    if created_at.tzinfo is None:
        created_at = created_at.replace(tzinfo=timezone.utc)
    return (
        intent.status in {"created", "queued"}
        and intent.execution_status == "unknown"
        and created_at > hardware_retry_cutoff()
    )


def requires_physical_recovery(action: str | None) -> bool:
    return str(action or "").strip().lower() in RECOVERY_REQUIRED_ACTIONS


def hardware_recovery_required_predicate():
    """One recovery policy for manual controls, sequences and signal tests.

    A missing ACK is a temporary uncertainty, not a permanent channel lock.
    Once the ACK wait has timed out, an explicit new command is allowed to
    create a new intent/packet. Preserve the timed-out intent as historical
    evidence; this predicate only controls admission of the next command.
    """
    unresolved = or_(
        HardwareCommandIntent.status.in_(("unknown", "recovery_required", "publish_failed")),
        and_(
            HardwareCommandIntent.status.in_(("created", "queued")),
            HardwareCommandIntent.execution_status.in_(("unknown", "timeout")),
        ),
    )
    expired_without_ack = and_(
        HardwareCommandIntent.execution_status.in_(("unknown", "timeout")),
        HardwareCommandIntent.created_at <= hardware_retry_cutoff(),
    )
    # A timeout is the retry boundary for every command type, including
    # pulses and commands that were marked recovery_required by the timeout
    # handler. Do not let a lost ACK prevent an operator from explicitly
    # sending a fresh command with a fresh command_id.
    return and_(unresolved, HardwareCommandIntent.execution_status != "timeout", ~expired_without_ack)


async def has_hardware_recovery_required(
    db: AsyncSession,
    *,
    workspace_id: int,
    channel_id: int,
) -> bool:
    # A physical channel cannot be safely reused by another workspace while any
    # prior command still requires recovery.  Keep workspace_id in the API for
    # callers and migrations, but scope the safety lookup by channel globally.
    del workspace_id
    result = await db.execute(
        select(
            exists().where(
                HardwareCommandIntentChannel.channel_id == int(channel_id),
                HardwareCommandIntentChannel.command_id == HardwareCommandIntent.command_id,
                hardware_recovery_required_predicate(),
            )
        )
    )
    return bool(result.scalar())


async def list_hardware_recovery_required_channels(
    db: AsyncSession,
    *,
    channel_ids: list[int],
    action: str | None = None,
) -> set[int]:
    """Return blocked physical channels in one query for multi-channel admission."""
    normalized = {int(channel_id) for channel_id in channel_ids if int(channel_id) > 0}
    if not normalized:
        return set()
    # Keep the internal call signature compatible; all actions use one policy.
    del action
    result = await db.execute(
        select(HardwareCommandIntentChannel.channel_id).where(
            HardwareCommandIntentChannel.channel_id.in_(normalized),
            HardwareCommandIntentChannel.command_id == HardwareCommandIntent.command_id,
            hardware_recovery_required_predicate(),
        ).distinct()
    )
    return {int(channel_id) for channel_id in result.scalars().all()}


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
        intent.status = "recovery_required" if requires_physical_recovery(intent.action) else "unknown"
    if intents:
        await db.flush()
    return len(intents)


async def reconcile_orphaned_manual_hardware_command_intents(
    db: AsyncSession,
) -> int:
    """Fail closed for manual commands left in-flight by an API restart."""
    result = await db.execute(
        select(HardwareCommandIntent).where(
            HardwareCommandIntent.owner_kind == "manual",
            HardwareCommandIntent.status.in_(("created", "queued")),
        )
    )
    intents = list(result.scalars().all())
    for intent in intents:
        intent.status = "recovery_required" if requires_physical_recovery(intent.action) else "unknown"
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
    try:
        primary_channel_id = int(channel_id)
    except (TypeError, ValueError) as exc:
        raise ValueError("Hardware intent requires a valid primary channel_id") from exc
    if primary_channel_id <= 0:
        raise ValueError("Hardware intent requires a positive primary channel_id")
    channel_ids: list[int] = [primary_channel_id]
    has_multi_channel_scope = isinstance(payload, dict) and "channel_ids" in payload
    raw_channel_ids = payload.get("channel_ids") if isinstance(payload, dict) else None
    if has_multi_channel_scope and not isinstance(raw_channel_ids, list):
        raise ValueError("Hardware intent channel_ids must be a list")
    if has_multi_channel_scope and not raw_channel_ids:
        raise ValueError("Hardware intent channel_ids must not be empty")
    if isinstance(raw_channel_ids, list):
        for raw_channel_id in raw_channel_ids:
            try:
                normalized_channel_id = int(raw_channel_id)
            except (TypeError, ValueError) as exc:
                raise ValueError("Hardware intent channel_ids contains an invalid channel") from exc
            if normalized_channel_id > 0 and normalized_channel_id not in channel_ids:
                channel_ids.append(normalized_channel_id)
            elif normalized_channel_id <= 0:
                raise ValueError("Hardware intent channel_ids must be positive")
    db.add(intent)
    for intent_channel_id in channel_ids:
        db.add(
            HardwareCommandIntentChannel(
                command_id=str(command_id),
                channel_id=int(intent_channel_id),
            )
        )
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


async def mark_hardware_command_intent_completed(
    db: AsyncSession,
    *,
    command_id: str,
) -> None:
    await db.execute(
        update(HardwareCommandIntent)
        .where(
            HardwareCommandIntent.command_id == command_id,
            HardwareCommandIntent.status == "queued",
            HardwareCommandIntent.execution_status == "acknowledged",
        )
        .values(status="completed")
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
