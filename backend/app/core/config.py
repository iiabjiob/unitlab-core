from functools import lru_cache
from pydantic_settings import BaseSettings, SettingsConfigDict
from pathlib import Path


class Settings(BaseSettings):
    app_name: str = "FAT Simulator"
    description: str = "FAT Simulator API"
    version: str = "1.0.0"
    debug: bool = True

    postgres_user: str
    postgres_password: str
    postgres_db: str
    postgres_host: str = "localhost"
    postgres_port: int = 5432

    model_config = SettingsConfigDict(
        env_file=str(Path(__file__).parent.parent / ".env"),  # Путь к .env в корне проекта
        env_file_encoding="utf-8",
        extra="allow",
    )

    @property
    def database_url(self) -> str:
        return f"postgresql://{self.postgres_user}:{self.postgres_password}@{self.postgres_host}:{self.postgres_port}/{self.postgres_db}"


@lru_cache()
def get_settings() -> Settings:
    return Settings()

# Проверка: загружены ли переменные
if __name__ == "__main__":
    settings = get_settings()
    print(f"✅ POSTGRES_USER={settings.postgres_user}")
    print(f"✅ POSTGRES_DB={settings.postgres_db}")
    print(f"✅ DATABASE_URL={settings.database_url}")