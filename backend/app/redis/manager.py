import redis.asyncio as redis
from app.core.config import get_settings
from app.core.logger import get_logger

logger = get_logger("redis")
settings = get_settings()

class RedisManager:
    _client = None

    @classmethod
    async def start(cls):
        if cls._client is None:
            cls._client = redis.from_url(
                settings.redis_url,
                encoding="utf-8",
                decode_responses=True,
            )
            # Healthcheck
            try:
                pong = await cls._client.ping()
                if pong:
                    logger.info("✅ Connected to Redis!")
                else:
                    logger.error("❌ Redis ping failed!")
            except Exception as e:
                logger.error(f"❌ Redis connection failed: {e}")

    @classmethod
    async def stop(cls):
        if cls._client:
            await cls._client.close()
            cls._client = None

    @classmethod
    def get_client(cls):
        if not cls._client:
            raise RuntimeError("Redis client is not initialized!")
        return cls._client
