from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.device import Device  # или откуда у тебя модель
from datetime import datetime, timezone

async def register_if_not_exists(
    db: AsyncSession,
    unit_id: str,
    channels : int,
    firmvare_version : str | None = None,
    type: str | None = None,
    is_active: bool = True,
) -> Device:
    try:
        # 1. Проверяем, существует ли уже устройство
        result = await db.execute(select(Device).where(Device.unit_id == unit_id))
        device = result.scalar_one_or_none()

        if device:
            return device  # Уже зарегистрировано
        
        # 3. Создаём новое
        new_device = Device(
            unit_id=unit_id,
            type=type,
            channels=channels,
            firmvare_version=firmvare_version,
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
    
async def get_all_unit_ids(db: AsyncSession) -> list[str]:
    """
    Возвращает список всех unit_id из таблицы устройств.
    """
    result = await db.execute(select(Device.unit_id))
    unit_ids = [row[0] for row in result.fetchall()]
    return unit_ids