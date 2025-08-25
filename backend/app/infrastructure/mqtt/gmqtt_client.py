import asyncio
from gmqtt import Client as MQTTClient
from app.infrastructure.mqtt.topics import CORE_TOPICS
from app.infrastructure.mqtt.router import MqttRouter
from app.core.logger import get_logger

logger = get_logger("mqtt")

class UnitLabMqttClient:
    """
    Async MQTT client based on gmqtt.
    Handles connection, subscriptions, and message routing.
    """
    _instance = None

    @classmethod
    def get_instance(cls, client_id=None):
        if cls._instance is None:
            if not client_id:
                raise ValueError("UnitLabMqttClient not initialized! Pass client_id for first init.")
            cls._instance = cls(client_id)
        return cls._instance

    def __init__(self, client_id: str):
        if UnitLabMqttClient._instance is not None:
            raise RuntimeError("UnitLabMqttClient already initialized, use get_instance().")
        self.client = MQTTClient(client_id)
        self.connected = asyncio.Event()
        self.router = MqttRouter()

        # Register event handlers
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message
        self.client.on_subscribe = self.on_subscribe

    def publish(self, topic, payload, qos=0, retain=False):
        # gmqtt publish вернёт coroutine (awaitable), но можно вызывать fire-and-forget
        return self.client.publish(topic, payload, qos=qos, retain=retain)
    
    def subscribe_default_topics(self):
            """
            Subscribe to all required default MQTT topics.
            Call this after connection.
            """
            for topic in CORE_TOPICS:
                self.client.subscribe(topic)
                logger.info(f"📡 Subscribed to topic: {topic}")

    async def connect(self, host: str, port: int):
        """
        Connect to the MQTT broker.
        """
        await self.client.connect(host, port)
        logger.info("🚀 MQTT client started")

    async def disconnect(self):
        """
        Gracefully disconnect from the MQTT broker.
        """
        await self.client.disconnect()
        logger.info("🛑 MQTT client stopped")

    def on_connect(self, client, flags, rc, properties):
        """
        Called automatically when the client connects to the broker.
        """
        logger.info("✅ Connected to MQTT broker")
        self.connected.set()
        self.subscribe_default_topics()

    def on_disconnect(self, client, packet, exc=None):
        """
        Called automatically when the client disconnects from the broker.
        """
        logger.warning("⚠️ Disconnected from MQTT broker")
        self.connected.clear()

    def on_subscribe(self, client, mid, qos, properties):
        """
        Called when the client successfully subscribes to a topic.
        """
        logger.info(f"📡 Subscribed (mid={mid}, qos={qos})")

    def on_message(self, client, topic, payload, qos, properties):
        """
        Called for every incoming MQTT message.
        Will add message routing in the next step.
        """
        logger.debug(f"📥 Received: {topic} → {payload.decode(errors='replace')}")
        asyncio.create_task(self.router.route(topic, payload))

