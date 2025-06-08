import asyncio
from app.ws.manager import ws_manager
from app.services.db.device import get_all_unit_ids
from app.db.database import AsyncSessionLocal
from app.redis.manager import RedisManager

async def device_offline_checker():
    while True:
        # Получи все unit_id из БД
        async with AsyncSessionLocal() as session:
            unit_ids = await get_all_unit_ids(session)
            redis_client = RedisManager.get_client()

            for unit_id in unit_ids:
                
                online = await redis_client.exists(f"device:{unit_id}:online")
                if not online:
                    await ws_manager.broadcast(
                        "unit_states_channel",  # можешь подставить свою функцию/канал
                        {
                            "unit_id": unit_id,
                            "status": "offline",
                        }
                    )
            await asyncio.sleep(10)
