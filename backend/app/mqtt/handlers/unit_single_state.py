from app.mqtt import router
from app.ws.websocket_manager import ws_manager
from app.ws.ws_channels import unit_states_channel
from app.core.logger import get_logger

logger = get_logger("mqtt")

@router.route("+/state/+")
async def handle_unit_single_state(topic: str, payload: dict | str | bool):
    """
    Обрабатывает индивидуальные состояния выходов,
    пришедшие по топику вида: do-unit-58EC/state/3
    """
    parts = topic.split("/")
    unit_id = parts[0]
    pin_index = parts[2]  # номер выхода, например "3"

    logger.debug(f"📤 IN ← Received state from {unit_id}, pin {pin_index}: {payload}")

    # Формируем полезную нагрузку в виде словаря: {"3": true}
    await ws_manager.broadcast(
        unit_states_channel(unit_id),
        {pin_index: payload}
    )
