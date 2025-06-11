from app.mqtt.gmqtt_client import UnitLabMqttClient
from app.core.config import get_settings
from app.core.logger import get_logger

logger = get_logger("mqtt")
settings = get_settings()

class MqttManager:
    _instance: UnitLabMqttClient | None = None

    @classmethod
    async def start(cls):
        if cls._instance is None:
            client_id = "unitlab-core"
            cls._instance = UnitLabMqttClient.get_instance(client_id=client_id)
            await cls._instance.connect(
                host=settings.mqtt_host,
                port=settings.mqtt_port
            )
            await cls._instance.connected.wait()
            logger.info(f"📡 MQTT client '{client_id}' connected and subscribed.")

    @classmethod
    async def stop(cls):
        if cls._instance:
            await cls._instance.disconnect()
            cls._instance = None

    @classmethod
    def get_instance(cls) -> UnitLabMqttClient:
        if not cls._instance:
            raise RuntimeError("MQTT client is not initialized! Call MqttManager.start() first.")
        return cls._instance
