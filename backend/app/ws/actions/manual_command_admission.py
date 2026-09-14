from __future__ import annotations

from fastapi import WebSocket
from sqlalchemy import select

from app.infrastructure.db.database import AsyncSessionLocal
from app.infrastructure.redis.manager import RedisManager
from app.models.channel import Channel
from app.models.device import Device
from app.services.command_queue_service import enqueue_ao_command, enqueue_do_command
from app.services.hardware_command_admission import HardwareChannelLease, HardwareCommandAdmission
from app.services.hardware_command_intent import (
    mark_hardware_command_intent_delivery_failure,
    mark_hardware_command_intent_queued,
    record_hardware_command_intent,
)
from app.schemas.ws.messages import SetAoCommandMessage, SetDoCommandMessage
from uuid import uuid4


async def _result(ws: WebSocket, *, command_id: str | None, delivery: str, reason: str | None = None) -> None:
    await ws.send_json({
        "event": "hardware_command_result",
        "command_id": command_id,
        "delivery": delivery,
        "execution": "unknown",
        "reason": reason,
    })


async def _channel(workspace_id: int, channel_id: int, unit_id: str, channel_index: int, expected_type: str):
    async with AsyncSessionLocal() as session:
        result = await session.execute(
            select(Channel)
            .join(Device, Device.id == Channel.device_id)
            .where(
                Channel.id == channel_id,
                Channel.channel_index == channel_index,
                Device.unit_id == unit_id,
            )
        )
        channel = result.scalar_one_or_none()
        if channel is None or not str(channel.channel_type).lower().startswith(expected_type):
            return None
        return channel


async def _enqueue_manual(
    ws: WebSocket,
    *,
    workspace_id: int,
    channel_id: int,
    unit_id: str,
    device_id: int,
    action: str,
    payload: dict,
    sender,
) -> None:
    redis = RedisManager.get_instance()
    owner_id = f"manual:{id(ws)}"
    admission = HardwareCommandAdmission(redis)
    lease = await admission.acquire(channel_id=channel_id, owner_kind="manual", owner_id=owner_id)
    if lease is None:
        await _result(ws, command_id=None, delivery="rejected", reason="channel_lease_busy")
        return

    command_id = uuid4().hex
    try:
        async with AsyncSessionLocal() as session:
            await record_hardware_command_intent(
                session,
                command_id=command_id,
                workspace_id=workspace_id,
                job_id=None,
                attempt_id=None,
                owner_kind="manual",
                owner_id=owner_id,
                device_id=device_id,
                channel_id=channel_id,
                unit_id=unit_id,
                action=action,
                payload=payload,
                fencing_epoch=lease.fencing_epoch,
            )
            await session.commit()
        await sender(command_id)
    except Exception:
        async with AsyncSessionLocal() as session:
            await mark_hardware_command_intent_delivery_failure(
                session,
                command_id=command_id,
                status="recovery_required" if action == "restore" else "publish_failed",
            )
            await session.commit()
        await _result(ws, command_id=command_id, delivery="rejected", reason="publish_failed")
        return
    finally:
        await admission.release(lease)
    await _result(ws, command_id=command_id, delivery="queued")


async def handle_manual_do(ws: WebSocket, msg: SetDoCommandMessage) -> None:
    if msg.mode.name not in {"SET_SINGLE_BIT", "SET_PULSE_BIT"} or msg.channel_id is None or msg.ch is None:
        await _result(ws, command_id=None, delivery="rejected", reason="single_channel_admission_required")
        return
    channel = await _channel(msg.workspace_id, msg.channel_id, msg.unit_id, msg.ch, "do")
    if channel is None:
        await _result(ws, command_id=None, delivery="rejected", reason="channel_scope_invalid")
        return
    await _enqueue_manual(
        ws,
        workspace_id=msg.workspace_id,
        channel_id=channel.id,
        unit_id=msg.unit_id,
        device_id=channel.device_id,
        action="do_set",
        payload=msg.model_dump(mode="json"),
        sender=lambda command_id: enqueue_do_command(
            unit_id=msg.unit_id,
            mode=msg.mode,
            ch=msg.ch,
            value=msg.value,
            pulse_ms=msg.pulse_ms,
            correlation_id=str(uuid4()),
            command_id=command_id,
        ),
    )


async def handle_manual_ao(ws: WebSocket, msg: SetAoCommandMessage) -> None:
    channel = await _channel(msg.workspace_id, msg.channel_id, msg.unit_id, msg.ch, "ao")
    if channel is None:
        await _result(ws, command_id=None, delivery="rejected", reason="channel_scope_invalid")
        return
    await _enqueue_manual(
        ws,
        workspace_id=msg.workspace_id,
        channel_id=channel.id,
        unit_id=msg.unit_id,
        device_id=channel.device_id,
        action="ao_set",
        payload=msg.model_dump(mode="json"),
        sender=lambda command_id: enqueue_ao_command(
            unit_id=msg.unit_id,
            ch=msg.ch,
            value=msg.value,
            correlation_id=str(uuid4()),
            command_id=command_id,
        ),
    )
