from __future__ import annotations

import asyncio
import time
from datetime import datetime, timezone

from sqlalchemy import select, update
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.hardware_command import HardwareCommandIntent


async def wait_for_hardware_command_acks(
    db: AsyncSession,
    *,
    command_ids: list[str],
    timeout_ms: int = 3000,
) -> dict[str, str]:
    """Wait for terminal ACK states without treating queue publication as success."""
    pending = {str(command_id) for command_id in command_ids if str(command_id).strip()}
    if not pending:
        return {}
    deadline = time.monotonic() + max(100, int(timeout_ms)) / 1000
    states: dict[str, str] = {}
    while pending and time.monotonic() < deadline:
        result = await db.execute(
            select(HardwareCommandIntent.command_id, HardwareCommandIntent.execution_status).where(
                HardwareCommandIntent.command_id.in_(pending)
            )
        )
        for command_id, execution_status in result.all():
            state = str(execution_status or "unknown")
            states[str(command_id)] = state
            if state in {"acknowledged", "negative_ack"}:
                pending.discard(str(command_id))
        if pending:
            await asyncio.sleep(0.05)
    if pending:
        await db.execute(
            update(HardwareCommandIntent)
            .where(
                HardwareCommandIntent.command_id.in_(pending),
                HardwareCommandIntent.execution_status == "unknown",
            )
            .values(execution_status="timeout")
        )
        await db.commit()
        for command_id in pending:
            states[command_id] = "timeout"
    return states


async def record_hardware_command_ack(
    db: AsyncSession,
    *,
    command_id: str,
    unit_id: str,
    packet_id: int,
    status: str,
    error: str,
) -> bool:
    result = await db.execute(
        select(HardwareCommandIntent).where(
            HardwareCommandIntent.command_id == command_id,
            HardwareCommandIntent.unit_id == unit_id,
        )
    )
    intent = result.scalar_one_or_none()
    if intent is None:
        return False
    if (intent.execution_status or "unknown") != "unknown":
        return False

    intent.execution_status = "acknowledged" if status == "OK" else "negative_ack"
    intent.ack_packet_id = int(packet_id)
    intent.ack_status = str(status)
    intent.ack_error = str(error)
    intent.ack_received_at = datetime.now(timezone.utc)
    await db.commit()
    return True
