from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.device import Device  # или откуда у тебя модель
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
        
        # 2. Парсим тип устройства
        device_type = parse_device_type(unit_id)

        # 3. Создаём новое
        new_device = Device(
            unit_id=unit_id,
            type=device_type,
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

def parse_device_type(unit_id: str) -> str:
    """
    Извлекает тип устройства из unit_id.
    Например:
    do-unit-XXXX -> DO
    di-unit-YYYY -> DI
    """
    if unit_id.startswith("do-unit-"):
        return "DO"
    elif unit_id.startswith("di-unit-"):
        return "DI"
    # Можно расширить другие типы:
    # elif unit_id.startswith("ai-unit-"):
    #     return "AI"
    else:
        return "UNKNOWN"  # если не распознали