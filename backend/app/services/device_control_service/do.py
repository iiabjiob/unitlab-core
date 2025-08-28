from app.infrastructure.mqtt import topics
from app.infrastructure.protocol.modes import Cmd
from app.infrastructure.protocol.encode import bit as bit_encode
from app.infrastructure.protocol.packet_io import PacketBuilder
from app.infrastructure.protocol.packet_structures import CmdSetSingleBit, CmdSetAllBit, CmdSetPairBit
from app.infrastructure.mqtt.gmqtt_client import UnitLabMqttClient
from app.core.logger import get_logger

logger = get_logger("mqtt")

def set_do_command_now(unit_id: str, action: dict):
    """
    action = {
      "mode": "SET_SINGLE_BIT",
      "ch": 1,
      "value": 1
    }
    """
    topic = topics.cmd('do', unit_id)

    mode = Cmd[action["mode"]]
    if mode == Cmd.SET_SINGLE_BIT:
        payload = bit_encode.cmd_set_single(CmdSetSingleBit(ch=action["ch"], value=action["value"]))
    elif mode == Cmd.SET_ALL_BIT:
        payload = bit_encode.cmd_set_all(CmdSetAllBit(bitmask=action["bitmask"]))
    elif mode == Cmd.SET_PAIR_BIT:
        payload = bit_encode.cmd_set_pair(
            CmdSetPairBit(chA=action["chA"], chB=action["chB"], state2b=action["state2b"])
        )
    else:
        raise ValueError(f"Unsupported DO command mode {action['mode']}")

    builder = PacketBuilder()
    builder.build(mode, packet_id=0, ts=0, payload=payload)
    data = builder.to_bytes()

    logger.info(f"📤 OUT → {topic} | payload={data.hex().upper()} ({mode.name})")
    UnitLabMqttClient.get_instance().publish(topic, data)
