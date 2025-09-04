from app.infrastructure.mqtt import topics
from app.infrastructure.protocol.modes import Cmd
from app.infrastructure.protocol.encode import bit as bit_encode
from app.infrastructure.protocol.packet_io import PacketBuilder
from app.infrastructure.protocol.packet_structures import CmdSetSingleBit, CmdSetAllBit, CmdSetPairBit
from app.infrastructure.mqtt.manager import MqttManager
from app.core.logger import get_logger

logger = get_logger("mqtt")

def set_do_command_now(
    unit_id: str,
    mode: Cmd,
    ch: int | None = None,
    value: int | None = None,
    bitmask: int | None = None,
    chA: int | None = None,
    chB: int | None = None,
    state2b: int | None = None,
    delay_before_ms: int = 0,
    pulse_ms: int = 0,
    repeat: int = 0,
):
    topic = topics.cmd("do", unit_id)

    if mode == Cmd.SET_SINGLE_BIT:
        if ch is None or value is None:
            raise ValueError("SET_SINGLE_BIT requires ch and value")
        payload = bit_encode.cmd_set_single(CmdSetSingleBit(ch=ch, value=value))

    elif mode == Cmd.SET_ALL_BIT:
        if bitmask is None:
            raise ValueError("SET_ALL_BIT requires bitmask")
        payload = bit_encode.cmd_set_all(CmdSetAllBit(bitmask=bitmask))

    elif mode == Cmd.SET_PAIR_BIT:
        if chA is None or chB is None or state2b is None:
            raise ValueError("SET_PAIR_BIT requires chA, chB, state2b")
        payload = bit_encode.cmd_set_pair(CmdSetPairBit(chA=chA, chB=chB, state2b=state2b))

    else:
        raise ValueError(f"Unsupported DO command mode {mode}")

    builder = PacketBuilder()
    builder.build(mode, packet_id=0, ts=0, payload=payload)
    data = builder.to_bytes()

    logger.info(f"📤 OUT → {topic} | payload={data.hex().upper()} ({mode.name})")
    MqttManager.get_instance().publish(topic, data)
