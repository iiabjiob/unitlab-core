from app.mqtt.gmqtt_client import UnitLabMqttClient
from app.core.config import get_settings
from app.core.logger import get_logger

logger = get_logger("mqtt")
settings = get_settings()

class MqttManager:
    _client: UnitLabMqttClient = None

    @classmethod
    async def start(cls):
        if cls._client is None:
            unit_id = "unitlab-core"
            cls._client = UnitLabMqttClient.get_instance(client_id=unit_id)
            await cls._client.connect(
                host=settings.mqtt_host,
                port=settings.mqtt_port
            )
            await cls._client.connected.wait()
            logger.info(f"📡 MQTT client '{unit_id}' connected and subscribed.")

    @classmethod
    async def stop(cls):
        if cls._client:
            await cls._client.disconnect()
            cls._client = None

    @classmethod
    def get_client(cls) -> UnitLabMqttClient:
        if not cls._client:
            raise RuntimeError("MQTT client is not initialized!")
        return cls._client
