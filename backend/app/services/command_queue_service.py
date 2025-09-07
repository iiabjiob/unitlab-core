# app/services/command_queue_service.py
from app.infrastructure.mqtt import topics
from app.infrastructure.protocol.packet_io import PacketBuilder
from app.infrastructure.protocol.modes import Cmd, State, Sys
from app.infrastructure.protocol.encode import (
    bit as bit_encode,
    afloat as float_encode,
)
from app.infrastructure.protocol.packet_structures import (
    CmdSetSingleBit,
    CmdSetAllBit,
    CmdSetPairBit,
    CmdSetPulseBit,
    CmdSetSingleFloat,
)
from app.core.message_bus import MessageBus, OutboundCmdMsg
from app.core.logger import get_logger

logger = get_logger("cmdq")

# ---------------- Packet ID generator ----------------
_packet_id = 0
def next_packet_id() -> int:
    global _packet_id
    _packet_id = 1 if _packet_id >= 0xFFFF else _packet_id + 1
    return _packet_id


# ---------------- DO Commands ----------------
async def enqueue_do_command(
    unit_id: str,
    mode: Cmd,
    ch: int | None = None,
    value: int | None = None,
    bitmask: int | None = None,
    chA: int | None = None,
    chB: int | None = None,
    state2b: int | None = None,
    pulse_ms: int = 0,
    correlation_id: str | None = None,
):
    topic = topics.cmd(unit_id)

    if mode == Cmd.SET_SINGLE_BIT:
        payload = bit_encode.cmd_set_single(CmdSetSingleBit(ch=ch, value=value))  # type: ignore
    elif mode == Cmd.SET_ALL_BIT:
        payload = bit_encode.cmd_set_all(CmdSetAllBit(bitmask=bitmask))  # type: ignore
    elif mode == Cmd.SET_PAIR_BIT:
        payload = bit_encode.cmd_set_pair(CmdSetPairBit(chA=chA, chB=chB, state2b=state2b))  # type: ignore
    elif mode == Cmd.SET_PULSE_BIT:
        payload = bit_encode.cmd_set_pulse(CmdSetPulseBit(ch=ch, value=value, pulse_ms=pulse_ms))  # type: ignore
    else:
        raise ValueError(f"Unsupported DO command mode {mode}")

    pid = next_packet_id()
    builder = PacketBuilder()
    builder.build(mode, packet_id=pid, ts=0, payload=payload)
    data = builder.to_bytes()

    msg = OutboundCmdMsg(
        topic=topic,
        payload=data,
        qos=0,
        retain=False,
        correlation_id=correlation_id,
        packet_id=pid,
    )

    await MessageBus.get_instance().outbound_cmd_q.put(msg)

    logger.info(f"🧺 Queued DO → {topic} | pid={pid} ({mode.name}) {data.hex().upper()}")


# ---------------- AO Commands ----------------
async def enqueue_ao_command(unit_id: str, ch: int, value: float, correlation_id: str | None = None):
    topic = topics.cmd(unit_id)

    mode = Cmd.SET_SINGLE_FLOAT
    payload = float_encode.cmd_set_single(CmdSetSingleFloat(ch=ch, value=value))

    pid = next_packet_id()
    builder = PacketBuilder()
    builder.build(mode, packet_id=pid, ts=0, payload=payload)
    data = builder.to_bytes()

    msg = OutboundCmdMsg(
        topic=topic,
        payload=data,
        qos=0,
        retain=False,
        correlation_id=correlation_id,
        packet_id=pid,
    )
        
    await MessageBus.get_instance().outbound_cmd_q.put(msg)
    
    logger.info(f"🧺 Queued AO → {topic} | pid={pid} ({mode.name}) {data.hex().upper()}")


# ---------------- STATE Requests ----------------
async def enqueue_request_state(
    unit_id: str,
    mode: State,
    ch: int | None = None,
    correlation_id: str | None = None,
):
    topic = topics.req_state(unit_id)

    if mode in (State.REQ_SINGLE_BIT, State.REQ_SINGLE_FLOAT):
        if ch is None:
            raise ValueError(f"{mode.name} requires channel number")
        payload = bytes([ch])
    else:
        payload = b""

    pid = next_packet_id()
    builder = PacketBuilder()
    builder.build(mode, packet_id=pid, ts=0, payload=payload)
    data = builder.to_bytes()

    msg = OutboundCmdMsg(
        topic=topic,
        payload=data,
        qos=0,
        retain=False,
        correlation_id=correlation_id,
        packet_id=pid,
    )
        
    await MessageBus.get_instance().outbound_cmd_q.put(msg)
    
    logger.info(f"🧺 Queued STATE REQ → {topic} | pid={pid} ({mode.name}) {data.hex().upper()}")


# ---------------- SCAN ----------------
async def enqueue_scan_devices(correlation_id: str | None = None, unit_id: str | None = None):
    
    # decide topic: broadcast vs unicast
    if unit_id:
        topic = topics.info(unit_id)
    else:
        topic = topics.DEVICE_SCAN

    pid = next_packet_id()
    builder = PacketBuilder()
    builder.build(Sys.SCAN, packet_id=pid, ts=0, payload=b"")
    payload = builder.to_bytes()

    msg = OutboundCmdMsg(
        topic=topic,
        payload=payload,
        qos=0,
        retain=False,
        correlation_id=correlation_id,
        packet_id=pid,
    )
        
    await MessageBus.get_instance().outbound_cmd_q.put(msg)
    
    logger.info(f"🧺 Queued SCAN → {topic} | pid={pid} {payload.hex().upper()}")
