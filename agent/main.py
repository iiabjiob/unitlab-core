import logging
import asyncio
from redis_agent import RedisHostAgent

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    agent = RedisHostAgent()
    try:
        asyncio.run(agent.start())
    except KeyboardInterrupt:
        print("Exiting agent...")
