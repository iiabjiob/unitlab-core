# app/infrastructure/mqtt/manager.py
from __future__ import annotations

from typing import Iterable, Optional

from app.infrastructure.mqtt.gmqtt_client import OnMessageAsync, UnitLabMqttClient
from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.mqtt.topics import CORE_TOPICS

logger = get_logger("mqtt")
settings = get_settings()

class MqttManager:
    """Application-level MQTT manager (singleton)."""
    _instance: UnitLabMqttClient | None = None

    @classmethod
    async def start(
        cls,
        *,
        client_id: str = "unitlab-core",
        subscriptions: Optional[Iterable[str]] = CORE_TOPICS,
        on_message: OnMessageAsync | None = None,
    ) -> UnitLabMqttClient:
        if cls._instance is not None:
            return cls._instance

        client = UnitLabMqttClient(client_id)
        if on_message:
            client.set_on_message(on_message)

        await client.connect(host=settings.mqtt_host, port=settings.mqtt_port)
        await client.connected.wait()

        if subscriptions:
            for topic in subscriptions:
                client.subscribe(topic)
                logger.info(f"📡 Subscribing to topic: {topic}")

        logger.info(f"✅ MQTT client '{client_id}' connected and subscribed.")
        cls._instance = client
        return client

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
