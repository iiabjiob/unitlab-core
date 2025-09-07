# app/infrastructure/mqtt/manager.py
import asyncio, time
from app.infrastructure.mqtt.gmqtt_client import UnitLabMqttClient
from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.mqtt.topics import CORE_TOPICS
from app.core.message_bus import MessageBus, InboundMqttMsg

logger = get_logger("mqtt")
settings = get_settings()

class MqttManager:
    """Application-level MQTT manager (singleton)."""
    _instance: UnitLabMqttClient | None = None

    @classmethod
    async def start(cls):
        if cls._instance is not None:
            return

        client_id = "unitlab-core"
        client = UnitLabMqttClient(client_id)

        # Register manager-level on_message hook
        async def _enqueue_inbound(topic: str, payload: bytes, qos: int, properties: object):
                        
            """Manager-owned async handler that pushes messages into inbound queue."""
            bus = MessageBus.get_instance()
            msg = InboundMqttMsg(
                topic=topic,
                payload=payload,
                qos=qos,
                ts_ms=int(time.time() * 1000),
            )
            await bus.inbound_mqtt_q.put(msg)

        client.set_on_message(_enqueue_inbound)

        await client.connect(host=settings.mqtt_host, port=settings.mqtt_port)
        await client.connected.wait()

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
