import redis.asyncio as redis
from app.core.config import get_settings
from app.core.logger import get_logger

logger = get_logger("redis")
settings = get_settings()

class RedisManager:
    _instance: redis.Redis | None = None

    @classmethod
    async def start(cls):
        if cls._instance is None:
            cls._instance = redis.from_url(
                settings.redis_url,
                encoding="utf-8",
                decode_responses=True,
            )
            try:
                if await cls._instance.ping():
                    logger.info("✅ Connected to Redis!")
                else:
                    logger.error("💥 Redis ping failed!")
            except Exception as e:
                logger.error(f"💥 Redis connection failed: {e}")
                cls._instance = None

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
