# app/mqtt/handlers/device_register_handler.py

from app.db.database import AsyncSessionLocal
from app.services.db.device import register_if_not_exists
from app.core.logger import get_logger

logger = get_logger("device")

async def handle_device_register_message(topic: str, payload: dict):
    """
    Обрабатывает регистрацию устройства из MQTT.
    """
    unit_id = topic.split("/")[-1]

    async with AsyncSessionLocal() as session:
        try:
            
            device = await register_if_not_exists(
                db=session,
                unit_id=unit_id,
                type_=payload.get("type"),
                is_active=True
            )
            logger.info(f"✅ Registered device: {device.unit_id}")
        except Exception as e:
            await session.rollback()
            logger.error(f"❌ Error registering device '{unit_id}': {e}")
