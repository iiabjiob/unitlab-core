# seed_devices.py
import asyncio
from datetime import datetime, timezone

from app.db.database import Base, engine, AsyncSessionLocal
from app.models.device import Device


devices_data = [
    {"unit_id": "esp32-01", "type": "DI", "is_active": True, "created_at": datetime.now(timezone.utc)},
    {"unit_id": "esp32-02", "type": "DO", "is_active": True, "created_at": datetime.now(timezone.utc)},
    {"unit_id": "esp32-03", "type": "DI", "is_active": False, "created_at": datetime.now(timezone.utc)},
]


async def seed():
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)

    async with AsyncSessionLocal() as session:
        await insert_devices(session)


async def insert_devices(session):
    for data in devices_data:
        existing = await session.get(Device, data["unit_id"])
        if not existing:
            session.add(Device(**data))
    await session.commit()
    print("Devices seeded.")


if __name__ == "__main__":
    asyncio.run(seed())
