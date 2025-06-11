import asyncio
from app.ws.ws_manager import WebSocketManager
from app.redis.redis_manager import RedisManager
from app.ws.ws_channels import unit_states_channel

async def device_offline_checker():
    redis_client = RedisManager.get_instance()
    ws_manager = WebSocketManager.get_instance()

    online_devices_prev = set()
    
    while True:
        # Получаем все ключи device:*:online
        keys = await redis_client.keys("device:*:online")
        current_online_devices = set()

        for key in keys:
            # Извлекаем unit_id из ключа, предполагая формат device:<unit_id>:online
            parts = key.decode() if isinstance(key, bytes) else key
            try:
                unit_id = parts.split(":")[1]
                current_online_devices.add(unit_id)
            except IndexError:
                continue

        # Определяем, кто **был онлайн, но теперь пропал**
        offline_devices = online_devices_prev - current_online_devices

        for unit_id in offline_devices:
            device_type = await redis_client.get(f"device:{unit_id}:type") or "unknown"

            await ws_manager.broadcast(
                unit_states_channel(unit_id, device_type),
                {
                    "unit_id": unit_id,
                    "device_type": device_type,
                    "status": "offline",
                }
            )


        # Обновляем сохранённое состояние
        online_devices_prev = current_online_devices

        await asyncio.sleep(10)