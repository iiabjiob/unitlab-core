import asyncio
from sqlalchemy import text

from app.infrastructure.db.database import engine
from app.core.logger import get_logger

logger = get_logger("health.db")


async def wait_for_database(
    max_attempts: int = 10,
    base_delay: float = 1.5,
) -> None:
    """Wait until database becomes available or raise."""

    for attempt in range(1, max_attempts + 1):
        try:
            async with engine.begin() as conn:
                await conn.execute(text("SELECT 1"))
                logger.info("Database connection established")
                return
        except Exception as exc:
            logger.warning(
                "Database not ready (attempt %s/%s): %s",
                attempt,
                max_attempts,
                exc,
            )

            if attempt >= max_attempts:
                logger.error("Database did not become ready")
                raise

            await asyncio.sleep(base_delay * attempt)
