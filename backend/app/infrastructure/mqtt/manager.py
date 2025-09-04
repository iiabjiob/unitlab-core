from app.infrastructure.mqtt.gmqtt_client import UnitLabMqttClient
from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.mqtt.topics import CORE_TOPICS

logger = get_logger("mqtt")
settings = get_settings()


class MqttManager:
    """Application-level MQTT manager (singleton)."""

    _instance: UnitLabMqttClient | None = None

    @classmethod
    async def start(cls):
        if cls._instance is None:
            client_id = "unitlab-core"
            client = UnitLabMqttClient(client_id)
            await client.connect(
                host=settings.mqtt_host,
                port=settings.mqtt_port
            )

            await client.connected.wait()

            # ✅ Подписка теперь на уровне приложения
            for topic in CORE_TOPICS:
                client.subscribe(topic)
                logger.info(f"📡 Subscribing to topic: {topic}")

            logger.info(f"✅ MQTT client '{client_id}' connected and subscribed.")
            cls._instance = client

    @classmethod
    async def stop(cls):
        if cls._instance:
            await cls._instance.disconnect()
            cls._instance = None

    @classmethod
    def get_instance(cls) -> UnitLabMqttClient:
        if not cls._instance:
            raise RuntimeError("MQTT client not initialized! Call MqttManager.start() first.")
        return cls._instance
