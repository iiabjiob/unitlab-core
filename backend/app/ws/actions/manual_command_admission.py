from __future__ import annotations

import asyncio
import time
import redis.asyncio as redis_async
from collections.abc import Awaitable, Callable
from typing import Literal, Protocol, cast
from fastapi import WebSocket
from sqlalchemy import exists, select

from app.core.config import get_settings
from app.infrastructure.db.database import AsyncSessionLocal
from app.infrastructure.redis.manager import RedisManager
from app.models.channel import Channel
from app.models.device import Device
from app.models.signal_sheet import SignalAllocation
from app.infrastructure.protocol.modes import State
from app.services.command_queue_service import enqueue_ao_command, enqueue_do_command, enqueue_request_state
from app.services.hardware_command_admission import HardwareCommandAdmission
from app.services.hardware_command_intent import (
    mark_hardware_command_intent_delivery_failure,
    mark_hardware_command_intent_completed,
    mark_hardware_command_intent_queued,
    list_hardware_recovery_required_channels,
    record_hardware_command_intent,
)
from app.services.hardware_command_ack import wait_for_hardware_command_acks
from app.schemas.ws.messages import SetAoCommandMessage, SetDoCommandMessage
from app.schemas.ws.events import HardwareCommandResultEvent
from uuid import uuid4

settings = get_settings()
MANUAL_COMMAND_ACK_TIMEOUT_MS = 3000
MANUAL_READBACK_TIMEOUT_MS = 2000
MANUAL_BULK_READBACK_ATTEMPT_MS = 250


class _ManualRedis(Protocol):
    async def get(self, name: str) -> object: ...

    async def hget(self, name: str, key: str) -> object: ...

    async def ttl(self, name: str) -> int: ...


def _redis() -> _ManualRedis:
    return cast(_ManualRedis, cast(object, RedisManager.get_instance()))


def _to_int(value: object) -> int:
    return int(cast(str | int | float | bytes | bytearray, value))


def _to_float(value: object) -> float:
    return float(cast(str | int | float | bytes | bytearray, value))


async def _wait_for_manual_readback(
    redis: _ManualRedis,
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
                fresh = _to_int(await redis.get(f"device:{unit_id}:last_state_packet_id")) == int(packet_id)
            except (TypeError, ValueError):
                fresh = False
        if action == "ao_set":
            raw_value = await redis.hget(f"device:{unit_id}:ao", str(int(channel_index)))
            try:
                actual_value = _to_float(raw_value)
            except (TypeError, ValueError):
                actual_value = None
            matched = fresh and actual_value is not None and abs(actual_value - float(expected_value)) <= 0.01
        else:
            raw_bitmask = await redis.get(f"device:{unit_id}:bitmask")
            try:
                bitmask = _to_int(raw_bitmask)
            except (TypeError, ValueError):
                bitmask = None
            matched = fresh and bitmask is not None and (1 if bitmask & (1 << int(channel_index)) else 0) == int(expected_value)
        if matched:
            return True
        if time.monotonic() >= deadline:
            return False
        await asyncio.sleep(0.05)


async def _wait_for_manual_bitmask_readback(
    redis: _ManualRedis,
    *,
    unit_id: str,
    expected_bitmask: int,
    channel_count: int,
    timeout_ms: int,
    packet_id: int,
) -> bool:
    if not 1 <= int(channel_count) <= 32:
        return False

    scope_mask = 0xFFFFFFFF if channel_count == 32 else (1 << channel_count) - 1
    expected = int(expected_bitmask) & scope_mask
    deadline = time.monotonic() + max(100, int(timeout_ms)) / 1000
    while True:
        try:
            fresh = _to_int(await redis.get(f"device:{unit_id}:last_state_packet_id")) == int(packet_id)
        except (TypeError, ValueError):
            fresh = False
        try:
            actual = _to_int(await redis.get(f"device:{unit_id}:bitmask")) & scope_mask
        except (TypeError, ValueError):
            actual = None
        if fresh and actual == expected:
            return True
        if time.monotonic() >= deadline:
            return False
        await asyncio.sleep(0.05)


async def _result(
    ws: WebSocket,
    *,
    command_id: str | None,
    delivery: Literal["queued", "rejected"],
    reason: str | None = None,
) -> None:
    await ws.send_json(
        HardwareCommandResultEvent(
            command_id=command_id,
            delivery=delivery,
            reason=reason,
        ).model_dump(mode="json")
    )


async def _channel(
    workspace_id: int,
    channel_id: int,
    unit_id: str,
    channel_index: int,
    expected_type: str,
    *,
    require_allocation: bool = True,
):
    conditions = [
        Channel.id == channel_id,
        Channel.channel_index == channel_index,
        Device.unit_id == unit_id,
    ]
    if require_allocation:
        conditions.append(
            exists().where(
                SignalAllocation.workspace_id == workspace_id,
                SignalAllocation.channel_id == Channel.id,
            )
        )
    async with AsyncSessionLocal() as session:
        result = await session.execute(
            select(Channel)
            .join(Device, Device.id == Channel.device_id)
            .where(*conditions)
        )
        channel = result.scalar_one_or_none()
        if channel is None or not str(channel.channel_type).lower().startswith(expected_type):
            return None
        return channel


async def _channels(
    workspace_id: int,
    channel_ids: list[int],
    unit_id: str,
    indexes: list[int],
    expected_type: str,
    *,
    require_allocation: bool = True,
):
    if not channel_ids or len(set(channel_ids)) != len(channel_ids):
        return None
    async with AsyncSessionLocal() as session:
        conditions = [Channel.id.in_(channel_ids), Device.unit_id == unit_id]
        if require_allocation:
            conditions.append(
                exists().where(
                    SignalAllocation.workspace_id == workspace_id,
                    SignalAllocation.channel_id == Channel.id,
                )
            )
        result = await session.execute(
            select(Channel).join(Device, Device.id == Channel.device_id).where(*conditions)
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
    redis = _redis()
    status = await redis.get(f"device:{unit_id}:status")
    if isinstance(status, bytes):
        status = status.decode("utf-8", errors="ignore")
    if str(status or "").strip().lower() != "online":
        return False
    last_seen_key = f"device:{unit_id}:last_seen"
    if not await redis.get(last_seen_key):
        return False
    # Redis TTL is independent of the application wall clock. Comparing the
    # stored epoch timestamp with time.time() rejects healthy devices after a
    # host clock step.
    return await redis.ttl(last_seen_key) > 0


async def _enqueue_manual(
    ws: WebSocket,
    *,
    workspace_id: int,
    channel_ids: list[int],
    unit_id: str,
    device_id: int,
    action: str,
    payload: dict[str, object],
    sender: Callable[[str], Awaitable[str]],
) -> None:
    redis = _redis()
    owner_id = f"manual:{id(ws)}"
    async with AsyncSessionLocal() as recovery_session:
        blocked_channels = await list_hardware_recovery_required_channels(
            recovery_session,
            channel_ids=channel_ids,
            action=action,
        )
    if blocked_channels:
        await _result(
            ws,
            command_id=None,
            delivery="rejected",
            reason="hardware_recovery_required",
        )
        return
    admission = HardwareCommandAdmission(cast(redis_async.Redis, cast(object, redis)))
    leases = await admission.acquire_many(channel_ids=channel_ids, owner_kind="manual", owner_id=owner_id)
    if leases is None:
        await _result(ws, command_id=None, delivery="rejected", reason="channel_lease_busy")
        return

    command_id = uuid4().hex
    execution_reason: str | None = None
    try:
        async with AsyncSessionLocal() as session:
            _ = await record_hardware_command_intent(
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
            is_current = getattr(admission, "is_current", None)
            if is_current is not None and not await is_current(leases[0]):
                raise RuntimeError("Hardware channel lease lost before command publish")
            _ = await sender(command_id)
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
                readback_scope_present = False
                readback_ok = False
                if action in {"do_set", "ao_set"}:
                    channel_index = payload.get("ch")
                    expected_value = payload.get("value")
                    if channel_index is not None and expected_value is not None:
                        readback_scope_present = True
                        readback_targets.append(
                            (
                                _to_int(channel_index),
                                _to_float(expected_value) if action == "ao_set" else _to_int(expected_value),
                            )
                        )
                elif action == "do_all" and payload.get("bitmask") is not None:
                    bitmask = _to_int(payload["bitmask"])
                    raw_channel_ids = payload.get("channel_ids")
                    channel_count = len(cast(list[object], raw_channel_ids)) if isinstance(raw_channel_ids, list) else 0
                    readback_scope_present = 1 <= channel_count <= 32
                    if readback_scope_present:
                        readback_deadline = time.monotonic() + MANUAL_READBACK_TIMEOUT_MS / 1000
                        attempt = 0
                        while not readback_ok and time.monotonic() < readback_deadline:
                            attempt += 1
                            remaining_ms = max(100, int((readback_deadline - time.monotonic()) * 1000))
                            try:
                                readback_packet_id = await enqueue_request_state(
                                    unit_id=unit_id,
                                    mode=State.REQ_ALL_BIT,
                                    correlation_id=f"manual:{command_id}:readback:all:{attempt}",
                                )
                                readback_ok = await _wait_for_manual_bitmask_readback(
                                    redis,
                                    unit_id=unit_id,
                                    expected_bitmask=bitmask,
                                    channel_count=channel_count,
                                    timeout_ms=min(MANUAL_BULK_READBACK_ATTEMPT_MS, remaining_ms),
                                    packet_id=readback_packet_id,
                                )
                            except Exception:  # noqa: BLE001
                                readback_ok = False
                elif action == "do_pair" and payload.get("state2b") is not None:
                    state2b = _to_int(payload["state2b"]) & 0b11
                    pair_indexes = (payload.get("chA"), payload.get("chB"))
                    pair_targets = (state2b & 0b01, (state2b >> 1) & 0b01)
                    readback_targets.extend(
                        (_to_int(index), _to_int(target))
                        for index, target in zip(pair_indexes, pair_targets)
                        if index is not None
                    )
                    readback_scope_present = bool(readback_targets)
                elif action == "do_pulse":
                    channel_index = payload.get("ch")
                    expected_value = payload.get("value")
                    if channel_index is not None and expected_value is not None:
                        readback_scope_present = True
                        readback_targets.append((_to_int(channel_index), _to_int(expected_value)))

                if readback_targets:
                    readback_ok = True
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
                if readback_ok and action == "do_pulse":
                    try:
                        pulse_ms = max(0, min(65535, _to_int(payload.get("pulse_ms") or 0)))
                        await asyncio.sleep(pulse_ms / 1000)
                        revert_packet_id = await enqueue_request_state(
                            unit_id=unit_id,
                            mode=State.REQ_SINGLE_BIT,
                            ch=_to_int(payload["ch"]),
                            correlation_id=f"manual:{command_id}:pulse-revert",
                        )
                        readback_ok = await _wait_for_manual_readback(
                            redis,
                            action=action,
                            unit_id=unit_id,
                            channel_index=_to_int(payload["ch"]),
                            expected_value=0,
                            timeout_ms=MANUAL_READBACK_TIMEOUT_MS,
                            packet_id=revert_packet_id,
                        )
                    except Exception:  # noqa: BLE001
                        readback_ok = False
                if not readback_ok:
                    execution_reason = "hardware_readback_timeout" if readback_scope_present else "readback_scope_missing"
                    await mark_hardware_command_intent_delivery_failure(
                        session,
                        command_id=command_id,
                        status="recovery_required",
                    )
                    await session.commit()
                else:
                    await mark_hardware_command_intent_completed(session, command_id=command_id)
                    await session.commit()
    except asyncio.CancelledError:
        async with AsyncSessionLocal() as session:
            await mark_hardware_command_intent_delivery_failure(
                session,
                command_id=command_id,
                status="recovery_required" if action in {"restore", "do_pulse"} else "unknown",
            )
            await session.commit()
        raise
    except Exception:
        async with AsyncSessionLocal() as session:
            await mark_hardware_command_intent_delivery_failure(
                session,
                command_id=command_id,
                status="recovery_required" if action in {"restore", "do_pulse"} else "unknown",
            )
            await session.commit()
        await _result(ws, command_id=command_id, delivery="rejected", reason="publish_failed")
        return
    finally:
        for lease in leases:
            _ = await admission.release(lease)
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
        valid_indexes = [int(index) for index in indexes if index is not None]
        channels = await _channels(
            msg.workspace_id,
            msg.channel_ids,
            msg.unit_id,
            valid_indexes,
            "do",
            require_allocation=False,
        )
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
            payload=cast(dict[str, object], cast(object, msg.model_dump(mode="json"))),
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
    if msg.mode.name == "SET_PULSE_BIT" and not 0 <= msg.pulse_ms <= 65535:
        await _result(ws, command_id=None, delivery="rejected", reason="pulse_duration_invalid")
        return
    channel = await _channel(
        msg.workspace_id,
        msg.channel_id,
        msg.unit_id,
        msg.ch,
        "do",
        require_allocation=False,
    )
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
        action="do_pulse" if msg.mode.name == "SET_PULSE_BIT" else "do_set",
        payload=cast(dict[str, object], cast(object, msg.model_dump(mode="json"))),
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
    channel = await _channel(
        msg.workspace_id,
        msg.channel_id,
        msg.unit_id,
        msg.ch,
        "ao",
        require_allocation=False,
    )
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
        payload=cast(dict[str, object], cast(object, msg.model_dump(mode="json"))),
        sender=lambda command_id: enqueue_ao_command(
            unit_id=msg.unit_id,
            ch=msg.ch,
            value=msg.value,
            correlation_id=str(uuid4()),
            command_id=command_id,
        ),
    )
