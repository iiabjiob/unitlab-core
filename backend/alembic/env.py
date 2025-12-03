from logging.config import fileConfig
from alembic import context

from sqlalchemy.ext.asyncio import AsyncEngine, create_async_engine
from sqlalchemy import pool

from app.infrastructure.db.database import Base

import app.models  # noqa: F401

from app.core.config import get_settings

settings = get_settings()


config = context.config

if config.config_file_name is not None:
    fileConfig(config.config_file_name)

# Полный async URL (НЕ удаляем +asyncpg!)
async_url = settings.database_url

safe_url = async_url.replace("%", "%%")

config.set_main_option("sqlalchemy.url", safe_url)

target_metadata = Base.metadata


def run_migrations_offline() -> None:
    """Run migrations in 'offline' mode."""

    context.configure(
        url=async_url,
        target_metadata=target_metadata,
        literal_binds=True,
        dialect_opts={"paramstyle": "named"},
        compare_type=True,
    )

    with context.begin_transaction():
        context.run_migrations()


async def run_migrations_online() -> None:
    """Run migrations in 'online' mode."""

    connectable = create_async_engine(
        async_url,
        poolclass=pool.NullPool,
    )

    async with connectable.connect() as connection:
        await connection.run_sync(
            lambda sync_conn: context.configure(
                connection=sync_conn,
                target_metadata=target_metadata,
                compare_type=True,
            )
        )

        await connection.run_sync(
            lambda sync_conn: context.run_migrations()
        )


if context.is_offline_mode():
    run_migrations_offline()
else:
    import asyncio

    asyncio.run(run_migrations_online())
