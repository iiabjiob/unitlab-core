from app.infrastructure.mqtt import topics
from app.infrastructure.protocol.modes import Cmd
from app.infrastructure.protocol.encode import afloat as float_encode
from app.infrastructure.protocol.packet_structures import CmdSetSingleFloat
from app.infrastructure.protocol.packet_io import PacketBuilder
from app.infrastructure.mqtt.manager import MqttManager
from app.core.logger import get_logger

logger = get_logger("mqtt")

def set_ao_command_now(unit_id: str, ch: int, value: float):

    topic = topics.cmd('ao', unit_id)

    mode = Cmd.SET_SINGLE_FLOAT
    payload = float_encode.cmd_set_single(CmdSetSingleFloat(ch=ch, value=value))

    builder = PacketBuilder()
    builder.build(mode, packet_id=0, ts=0, payload=payload)
    data = builder.to_bytes()

    logger.info(f"📤 OUT → {topic} | payload={data.hex().upper()} ({mode.name})")
    MqttManager.get_instance().publish(topic, data)
