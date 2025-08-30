from functools import lru_cache
from pydantic_settings import BaseSettings, SettingsConfigDict

class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="allow")

    app_version: str
    app_name: str
    description: str

    app_env: str
    debug: bool
    debug_level: str

    ap_ssid_prefix: str
    ap_password_prefix: str

    mqtt_host: str
    mqtt_port: int

    redis_host: str
    redis_port: int

    @property
    def database_url(self) -> str:
        return f"postgresql+asyncpg://{self.postgres_user}:{self.postgres_password}@{self.postgres_host}:{self.postgres_port}/{self.postgres_db}"
    
    @property
    def redis_url(self) -> str:
        return f"redis://{self.redis_host}:{self.redis_port}/0"
    
    heartbeat_ttl: int = 10  # seconds
    check_heartbeat_interval: int = 5 # seconds


@lru_cache
def get_settings():
    return Settings()