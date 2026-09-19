import redis.asyncio as redis
from collections.abc import Awaitable, Callable
from typing import cast
from app.core.config import get_settings
from app.core.logger import get_logger

logger = get_logger("redis")
settings = get_settings()

class RedisManager:
    _instance: redis.Redis | None = None

    @classmethod
    async def start(cls):
        if cls._instance is None:
            redis_factory = cast(Callable[..., redis.Redis], getattr(redis, "from_url"))
            cls._instance = redis_factory(
                settings.redis_url,
                encoding="utf-8",
                decode_responses=True,
            )
            try:
                ping = cast(Callable[[], Awaitable[bool]], getattr(cls._instance, "ping"))
                if await ping():
                    logger.info("✅ Connected to Redis!")
                else:
                    logger.error("💥 Redis ping failed!")
                    await cls._instance.close()
                    cls._instance = None
                    raise RuntimeError("Redis ping failed during startup")
            except Exception as e:
                logger.error(f"💥 Redis connection failed: {e}")
                if cls._instance is not None:
                    await cls._instance.close()
                cls._instance = None
                raise

    @classmethod
    async def stop(cls):
        if cls._instance:
            await cls._instance.close()
            cls._instance = None

    @classmethod
    def get_instance(cls) -> redis.Redis:
        if not cls._instance:
            raise RuntimeError("Redis client is not initialized! Call RedisManager.start() first.")
        return cls._instance
