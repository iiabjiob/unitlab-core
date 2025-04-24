import paho.mqtt.client as mqtt
import asyncio
from app.mqtt.dispatcher import dispatch
from app.mqtt.publisher import init_mqtt_client
from app.core.logger import get_logger

logger = get_logger("mqtt")

event_loop = None  # будет установлен при запуске

MQTT_HOST = "localhost"
MQTT_PORT = 1883

client = mqtt.Client()
init_mqtt_client(client)

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        logger.info("✅ Сonnected successfully")
    else:
        logger.error(f"❌ Сonnection failed with code {rc}")

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode()
    logger.debug(f"{topic} → {payload}")

    # Асинхронная пересылка по WebSocket в event_loop
    from app.mqtt.client import event_loop  # импортируем глобальный loop

    if event_loop:
        asyncio.run_coroutine_threadsafe(dispatch(topic, payload), event_loop)
    else:
        logger.warning("⚠️ No event loop available for dispatch!")
        
def start_mqtt():
    try:
        global event_loop
        event_loop = asyncio.get_event_loop()  # сохранить текущий loop

        client.connect(MQTT_HOST, MQTT_PORT, 60)
        client.loop_start()

        logger.info("🚀 Client started")
    except Exception as e:
        logger.error(f"❌ Startup error: {e}")

client.on_connect = on_connect
client.on_message = on_message