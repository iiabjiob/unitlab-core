from datetime import datetime, timezone
from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.channel import Channel


async def create_channel(
    db: AsyncSession,
    device_id: int,
    index: int,
    type: str,
    name: str | None = None,
) -> Channel:
    """
    Создаёт новый канал для устройства.
    """
    try:
        new_channel = Channel(
            device_id=device_id,
            index=index,
            type=type,
            name=name,
            created_at=datetime.now(timezone.utc),
        )
        db.add(new_channel)
        await db.commit()
        await db.refresh(new_channel)
        return new_channel

    except SQLAlchemyError as e:
        await db.rollback()
        raise RuntimeError(f"Database error during channel creation: {e}")


async def get_channel_by_id(db: AsyncSession, channel_id: int) -> Channel | None:
    """
    Возвращает канал по ID.
    """
    result = await db.execute(select(Channel).where(Channel.id == channel_id))
    return result.scalar_one_or_none()


async def get_channels_by_device(db: AsyncSession, device_id: int) -> list[Channel]:
    """
    Возвращает все каналы устройства.
    """
    result = await db.execute(select(Channel).where(Channel.device_id == device_id))
    return list(result.scalars().all())


async def update_channel(
    db: AsyncSession,
    channel_id: int,
    name: str | None = None,
    type: str | None = None,
) -> Channel | None:
    """
    Обновляет параметры канала.
    """
    result = await db.execute(select(Channel).where(Channel.id == channel_id))
    channel = result.scalar_one_or_none()

    if channel is None:
        return None

    if name is not None:
        channel.name = name
    if type is not None:
        channel.type = type

    await db.commit()
    await db.refresh(channel)
    return channel


async def delete_channel(db: AsyncSession, channel_id: int) -> bool:
    """
    Удаляет канал по ID. Возвращает True, если удалён.
    """
    result = await db.execute(select(Channel).where(Channel.id == channel_id))
    channel = result.scalar_one_or_none()

    if channel is None:
        return False

    await db.delete(channel)
    await db.commit()
    return True

async def register_or_update_channels(
    db: AsyncSession,
    device_id: int,
    num_channels: int,
    channel_type: str,
) -> list[Channel]:
    """
    Синхронизирует каналы устройства с заданным num_channels и типом.
    - Добавляет недостающие каналы
    - Удаляет лишние
    - Обновляет тип существующих каналов
    - Возвращает актуальный список каналов
    """

    # 1. Загружаем текущие каналы
    result = await db.execute(select(Channel).where(Channel.device_id == device_id))
    existing_channels = list(result.scalars().all())
    existing_indexes = {ch.index for ch in existing_channels}

    # 2. Добавляем недостающие каналы
    for i in range(num_channels):
        if i not in existing_indexes:
            new_channel = Channel(
                device_id=device_id,
                index=i,
                type=channel_type,
                created_at=datetime.now(timezone.utc),
            )
            db.add(new_channel)

    # 3. Обновляем тип существующих каналов (если вдруг изменился у устройства)
    for ch in existing_channels:
        if ch.type != channel_type:
            ch.type = channel_type

    # 4. Удаляем лишние каналы
    for ch in existing_channels:
        if ch.index >= num_channels:
            await db.delete(ch)

    await db.commit()

    # 5. Возвращаем актуальный список
    result = await db.execute(select(Channel).where(Channel.device_id == device_id))
    return list(result.scalars().all())