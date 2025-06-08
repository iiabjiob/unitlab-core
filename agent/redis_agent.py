import redis.asyncio as aioredis
import logging

class RedisHostAgent:
    def __init__(self, redis_url="redis://localhost:6379/0", ping_channel="agent:ping", response_channel="agent:ping:response"):
        self.redis_url = redis_url
        self.ping_channel = ping_channel
        self.response_channel = response_channel
        self.logger = logging.getLogger("host-agent")
        self.redis = None
        self.pubsub = None

    async def start(self):
        self.redis = aioredis.from_url(self.redis_url, decode_responses=True)
        self.pubsub = self.redis.pubsub()
        await self.pubsub.subscribe(self.ping_channel)
        self.logger.info(f"Subscribed to {self.ping_channel}")

        await self.run_loop()

    async def handle_ping(self, data):
        self.logger.info(f"Received PING: {data}")
        await self.redis.publish(self.response_channel, "pong")
        self.logger.info(f"Sent PONG to {self.response_channel}")

    async def run_loop(self):
        async for message in self.pubsub.listen():
            if message["type"] == "message":
                channel = message["channel"]
                data = message["data"]
                if channel == self.ping_channel:
                    await self.handle_ping(data)

    async def stop(self):
        if self.pubsub:
            await self.pubsub.unsubscribe(self.ping_channel)
            await self.pubsub.close()
        if self.redis:
            await self.redis.close()
