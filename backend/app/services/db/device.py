from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession

from app.db.models.device import Device  # или откуда у тебя модель
from datetime import datetime, timezone


async def register_if_not_exists(
    db: AsyncSession,
    unit_id: str,
    type_: str | None = None,
    is_active: bool = True,
) -> Device:
    try:
        # 1. Проверяем, существует ли уже устройство
        result = await db.execute(select(Device).where(Device.unit_id == unit_id))
        device = result.scalar_one_or_none()

        if device:
            return device  # Уже зарегистрировано

        # 2. Создаём новое
        new_device = Device(
            unit_id=unit_id,
            type=type_,
            is_active=is_active,
            created_at=datetime.now(timezone.utc)
        )

        db.add(new_device)
        await db.commit()
        await db.refresh(new_device)

        return new_device

    except SQLAlchemyError as e:
        await db.rollback()
        raise RuntimeError(f"Database error during registration: {e}")
