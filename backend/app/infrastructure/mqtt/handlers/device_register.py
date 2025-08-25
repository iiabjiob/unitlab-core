import json
from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.db.database import AsyncSessionLocal
from app.repositories.device_repository import register_if_not_exists
from app.infrastructure.mqtt import topics
from app.core.logger import get_logger
from app.core.config import get_settings

settings = get_settings()

logger = get_logger("device")

@registry.mqtt_handler(topics.DEVICE_REGISTER)
async def handle_device_register(topic: str, payload, match):
    """
    Обрабатывает регистрацию устройства из MQTT.
    """
    unit_id = topic.split("/")[-1]

    # Always decode payload to dict
    if isinstance(payload, bytes):
        try:
            payload = json.loads(payload.decode())
        except Exception:
            # Если payload пустой — делаем дефолтный dict для теста
            if not payload.strip() and settings.debug:
                payload = {}
            else:
                logger.error(f"❌ Invalid payload: {payload}")
                return

    type = payload.get("type", "test" if settings.debug else None)
    channels = payload.get("channels")
    firmvare_version = payload.get("firmvare_version")

    logger.debug(f"Registering device {unit_id} with type: {type}, payload: {payload}")

    async with AsyncSessionLocal() as session:
        try:
            device = await register_if_not_exists(
                db = session,
                unit_id = unit_id,
                channels = channels,
                firmvare_version = firmvare_version,
                type = type,
                is_active=True
            )
            logger.info(f"✅ Registered device: {device.unit_id}")
        except Exception as e:
            await session.rollback()
            logger.error(f"❌ Error registering device '{unit_id}': {e}")