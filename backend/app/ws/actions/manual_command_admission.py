from __future__ import annotations

import asyncio
import time
from typing import Any
from fastapi import WebSocket
from sqlalchemy import exists, select

from app.core.config import get_settings
from app.core.utils import to_int
from app.infrastructure.db.database import AsyncSessionLocal
from app.infrastructure.redis.manager import RedisManager
from app.models.channel import Channel
from app.models.device import Device
from app.models.signal_sheet import SignalAllocation
from app.infrastructure.protocol.modes import State
from app.services.command_queue_service import enqueue_ao_command, enqueue_do_command, enqueue_request_state
from app.services.hardware_command_admission import HardwareChannelLease, HardwareCommandAdmission
from app.services.hardware_command_intent import (
    mark_hardware_command_intent_delivery_failure,
    mark_hardware_command_intent_queued,
    record_hardware_command_intent,
)
from app.services.hardware_command_ack import wait_for_hardware_command_acks
from app.schemas.ws.messages import SetAoCommandMessage, SetDoCommandMessage
from app.schemas.ws.events import HardwareCommandResultEvent
from uuid import uuid4

settings = get_settings()
MANUAL_COMMAND_ACK_TIMEOUT_MS = 3000
MANUAL_READBACK_TIMEOUT_MS = 2000


async def _wait_for_manual_readback(
    redis: Any,
    *,
    action: str,
    unit_id: str,
    channel_index: int,
    expected_value: float | int,
    timeout_ms: int,
    packet_id: int | None = None,
) -> bool:
    deadline = time.monotonic() + max(100, int(timeout_ms)) / 1000
    while True:
        fresh = True
        if packet_id is not None:
            try:
                fresh = int(await redis.get(f"device:{unit_id}:last_state_packet_id")) == int(packet_id)
            except (TypeError, ValueError):
                fresh = False
        if action == "ao_set":
            raw_value = await redis.hget(f"device:{unit_id}:ao", str(int(channel_index)))
            try:
                actual_value = float(raw_value)
            except (TypeError, ValueError):
                actual_value = None
            matched = fresh and actual_value is not None and abs(actual_value - float(expected_value)) <= 0.01
        else:
            raw_bitmask = await redis.get(f"device:{unit_id}:bitmask")
            try:
                bitmask = int(raw_bitmask)
            except (TypeError, ValueError):
                bitmask = None
            matched = fresh and bitmask is not None and (1 if bitmask & (1 << int(channel_index)) else 0) == int(expected_value)
        if matched:
            return True
        if time.monotonic() >= deadline:
            return False
        await asyncio.sleep(0.05)


async def _result(
    ws: WebSocket,
    *,
    command_id: str | None,
    delivery: str,
    reason: str | None = None,
) -> None:
    await ws.send_json(
        HardwareCommandResultEvent(
            command_id=command_id,
            delivery=delivery,
            reason=reason,
        ).model_dump(mode="json")
    )


async def _channel(workspace_id: int, channel_id: int, unit_id: str, channel_index: int, expected_type: str):
    async with AsyncSessionLocal() as session:
        result = await session.execute(
            select(Channel)
            .join(Device, Device.id == Channel.device_id)
            .where(
                Channel.id == channel_id,
                Channel.channel_index == channel_index,
                Device.unit_id == unit_id,
                exists().where(
                    SignalAllocation.workspace_id == workspace_id,
                    SignalAllocation.channel_id == Channel.id,
                ),
            )
        )
        channel = result.scalar_one_or_none()
        if channel is None or not str(channel.channel_type).lower().startswith(expected_type):
            return None
        return channel


async def _channels(workspace_id: int, channel_ids: list[int], unit_id: str, indexes: list[int], expected_type: str):
    if not channel_ids or len(set(channel_ids)) != len(channel_ids):
        return None
    async with AsyncSessionLocal() as session:
        result = await session.execute(
            select(Channel)
            .join(Device, Device.id == Channel.device_id)
            .where(Channel.id.in_(channel_ids), Device.unit_id == unit_id)
            .where(
                exists().where(
                    SignalAllocation.workspace_id == workspace_id,
                    SignalAllocation.channel_id == Channel.id,
                )
            )
        )
        by_id = {int(channel.id): channel for channel in result.scalars().all()}
    if set(by_id) != set(channel_ids) or len(indexes) != len(channel_ids):
        return None
    ordered = [by_id[channel_id] for channel_id in channel_ids]
    if [int(channel.channel_index) for channel in ordered] != indexes:
        return None
    if any(not str(channel.channel_type).lower().startswith(expected_type) for channel in ordered):
        return None
    return ordered


async def _device_is_online(unit_id: str) -> bool:
    redis = RedisManager.get_instance()
    status = await redis.get(f"device:{unit_id}:status")
    if isinstance(status, bytes):
        status = status.decode("utf-8", errors="ignore")
    if str(status or "").strip().lower() != "online":
        return False
    last_seen = to_int(await redis.get(f"device:{unit_id}:last_seen"))
    if last_seen is None:
        return False
    age_ms = int(time.time() * 1000) - last_seen
    return 0 <= age_ms <= max(int(settings.heartbeat_ttl or 30), 1) * 1000


async def _enqueue_manual(
    ws: WebSocket,
    *,
    workspace_id: int,
    channel_ids: list[int],
    unit_id: str,
    device_id: int,
    action: str,
    payload: dict,
    sender,
) -> None:
    redis = RedisManager.get_instance()
    owner_id = f"manual:{id(ws)}"
    admission = HardwareCommandAdmission(redis)
    leases = await admission.acquire_many(channel_ids=channel_ids, owner_kind="manual", owner_id=owner_id)
    if leases is None:
        await _result(ws, command_id=None, delivery="rejected", reason="channel_lease_busy")
        return

    command_id = uuid4().hex
    execution_reason: str | None = None
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
                channel_id=channel_ids[0],
                unit_id=unit_id,
                action=action,
                payload={**payload, "channel_ids": channel_ids},
                fencing_epoch=leases[0].fencing_epoch,
            )
            await session.commit()
            await sender(command_id)
            await mark_hardware_command_intent_queued(session, command_id=command_id)
            await session.commit()
            ack_states = await wait_for_hardware_command_acks(
                session,
                command_ids=[command_id],
                timeout_ms=MANUAL_COMMAND_ACK_TIMEOUT_MS,
            )
            execution = str(ack_states.get(command_id) or "unknown")
            if execution == "negative_ack":
                execution_reason = "device_negative_ack"
            elif execution == "timeout":
                execution_reason = "hardware_ack_timeout"
            elif execution == "acknowledged":
                readback_targets: list[tuple[int, float | int]] = []
                if action in {"do_set", "ao_set"}:
                    channel_index = payload.get("ch")
                    expected_value = payload.get("value")
                    if channel_index is not None and expected_value is not None:
                        readback_targets.append(
                            (
                                int(channel_index),
                                float(expected_value) if action == "ao_set" else int(expected_value),
                            )
                        )
                elif action == "do_all" and payload.get("bitmask") is not None:
                    bitmask = int(payload["bitmask"])
                    readback_targets.extend(
                        (index, 1 if bitmask & (1 << index) else 0)
                        for index in range(len(payload.get("channel_ids") or []))
                    )
                elif action == "do_pair" and payload.get("state2b") is not None:
                    state2b = int(payload["state2b"]) & 0b11
                    pair_indexes = (payload.get("chA"), payload.get("chB"))
                    pair_targets = (state2b & 0b01, (state2b >> 1) & 0b01)
                    readback_targets.extend(
                        (int(index), int(target))
                        for index, target in zip(pair_indexes, pair_targets)
                        if index is not None
                    )

                readback_ok = bool(readback_targets)
                for target_index, target_value in readback_targets:
                    try:
                        readback_packet_id = await enqueue_request_state(
                            unit_id=unit_id,
                            mode=State.REQ_SINGLE_FLOAT if action == "ao_set" else State.REQ_SINGLE_BIT,
                            ch=target_index,
                            correlation_id=f"manual:{command_id}:readback:{target_index}",
                        )
                        target_ok = await _wait_for_manual_readback(
                            redis,
                            action=action,
                            unit_id=unit_id,
                            channel_index=target_index,
                            expected_value=target_value,
                            timeout_ms=MANUAL_READBACK_TIMEOUT_MS,
                            packet_id=readback_packet_id,
                        )
                    except Exception:  # noqa: BLE001
                        target_ok = False
                    readback_ok = readback_ok and target_ok
                if not readback_ok:
                    execution_reason = "hardware_readback_timeout" if readback_targets else "readback_scope_missing"
                    await mark_hardware_command_intent_delivery_failure(
                        session,
                        command_id=command_id,
                        status="recovery_required",
                    )
                    await session.commit()
    except asyncio.CancelledError:
        async with AsyncSessionLocal() as session:
            await mark_hardware_command_intent_delivery_failure(
                session,
                command_id=command_id,
                status="recovery_required" if action == "restore" else "unknown",
            )
            await session.commit()
        raise
    except Exception:
        async with AsyncSessionLocal() as session:
            await mark_hardware_command_intent_delivery_failure(
                session,
                command_id=command_id,
                status="recovery_required" if action == "restore" else "unknown",
            )
            await session.commit()
        await _result(ws, command_id=command_id, delivery="rejected", reason="publish_failed")
        return
    finally:
        for lease in leases:
            await admission.release(lease)
    await _result(
        ws,
        command_id=command_id,
        delivery="queued",
        reason=execution_reason,
    )


async def handle_manual_do(ws: WebSocket, msg: SetDoCommandMessage) -> None:
    if msg.mode.name not in {"SET_SINGLE_BIT", "SET_PULSE_BIT"}:
        if not msg.channel_ids:
            await _result(ws, command_id=None, delivery="rejected", reason="channel_ids_required")
            return
        indexes = [msg.chA, msg.chB] if msg.mode.name == "SET_PAIR_BIT" else list(range(len(msg.channel_ids)))
        if any(index is None for index in indexes):
            await _result(ws, command_id=None, delivery="rejected", reason="channel_indexes_required")
            return
        channels = await _channels(msg.workspace_id, msg.channel_ids, msg.unit_id, [int(index) for index in indexes], "do")
        if channels is None:
            await _result(ws, command_id=None, delivery="rejected", reason="channel_scope_invalid")
            return
        if not await _device_is_online(msg.unit_id):
            await _result(ws, command_id=None, delivery="rejected", reason="device_offline")
            return
        await _enqueue_manual(
            ws,
            workspace_id=msg.workspace_id,
            channel_ids=[int(channel.id) for channel in channels],
            unit_id=msg.unit_id,
            device_id=channels[0].device_id,
            action="do_pair" if msg.mode.name == "SET_PAIR_BIT" else "do_all",
            payload=msg.model_dump(mode="json"),
            sender=lambda command_id: enqueue_do_command(
                unit_id=msg.unit_id,
                mode=msg.mode,
                bitmask=msg.bitmask,
                chA=msg.chA,
                chB=msg.chB,
                state2b=msg.state2b,
                command_id=command_id,
            ),
        )
        return
    if msg.channel_id is None or msg.ch is None:
        await _result(ws, command_id=None, delivery="rejected", reason="channel_id_required")
        return
    channel = await _channel(msg.workspace_id, msg.channel_id, msg.unit_id, msg.ch, "do")
    if channel is None:
        await _result(ws, command_id=None, delivery="rejected", reason="channel_scope_invalid")
        return
    if not await _device_is_online(msg.unit_id):
        await _result(ws, command_id=None, delivery="rejected", reason="device_offline")
        return
    await _enqueue_manual(
        ws,
        workspace_id=msg.workspace_id,
        channel_ids=[channel.id],
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
    if not await _device_is_online(msg.unit_id):
        await _result(ws, command_id=None, delivery="rejected", reason="device_offline")
        return
    await _enqueue_manual(
        ws,
        workspace_id=msg.workspace_id,
        channel_ids=[channel.id],
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
