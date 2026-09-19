from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine
from sqlalchemy.orm import DeclarativeBase
from app.core.config import get_settings

settings = get_settings()

# Async SQLAlchemy engine
engine = create_async_engine(
    settings.database_url,
    future=True,
    echo=settings.db_echo,
    pool_pre_ping=True,
)

# Session factory for DB work
AsyncSessionLocal: async_sessionmaker[AsyncSession] = async_sessionmaker(
    bind=engine,
    expire_on_commit=False,
)



class Base(DeclarativeBase):
    """Typed declarative base for all SQLAlchemy models."""

    pass

# Dependency that yields an async session
async def get_db():
    async with AsyncSessionLocal() as session:
        yield session
