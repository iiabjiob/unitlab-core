from app.mqtt import router
from app.ws.websocket_manager import ws_manager
from app.ws.ws_channels import unit_states_channel
from app.core.logger import get_logger

logger = get_logger("mqtt")

@router.route("+/states")
async def handle_unit_states(topic: str, payload: dict):
    """
    Обрабатывает входящие состояния всех выходов устройства,
    пришедшие по топику вида: do-unit-58EC/states
    """
    unit_id = topic.split("/", 1)[0]  # do-unit-58EC

    logger.debug(f"📤 IN ← Received states from {unit_id}: {payload}")

    await ws_manager.broadcast(unit_states_channel(unit_id), payload)
