from pathlib import Path
import os
from urllib.parse import quote
from pydantic_settings import BaseSettings, SettingsConfigDict


REPO_ROOT = Path(__file__).resolve().parents[2]
APP_ENV = os.getenv("APP_ENV", "development").lower()
ENV_FILE = None if APP_ENV in ("production", "prod") else REPO_ROOT / ".env.dev"

print("ENV_FILE resolved to:", ENV_FILE)

if ENV_FILE and not ENV_FILE.exists():
    raise FileNotFoundError("❌ ENV FILE NOT FOUND: .env.dev")


model_config: dict = {"extra": "allow"}
if ENV_FILE:
    model_config["env_file"] = str(ENV_FILE)


class Settings(BaseSettings):
    model_config = SettingsConfigDict(**model_config)

    # ---- BASE ----
    app_version: str = "0.1.0"
    app_name: str = "UnitLab Backend"
    description: str = "Backend for UnitLab application"
    app_env: str = "production"
    debug: bool = False
    debug_level: str = "INFO"

    # ---- DB ----
    postgres_user: str
    postgres_password: str
    postgres_host: str
    postgres_port: int
    postgres_db: str

    # ---- MQTT ----
    mqtt_host: str
    mqtt_port: int
    mqtt_in_stream: str = "mqtt:inbound"
    mqtt_out_stream: str = "mqtt:outbound"
    mqtt_stream_maxlen: int = 10000

    # ---- Redis ----
    redis_host: str
    redis_port: int
    ws_events_channel: str = "ws:events"

    # ---- Heartbeat ----
    heartbeat_ttl: int = 30
    check_heartbeat_interval: int = 5

    # ---- Time Sync ----
    ntp_server_1: str | None = None
    ntp_server_2: str | None = None

    # ---- Database URL ----
    @property
    def database_url(self) -> str:
        user = quote(self.postgres_user)
        password = quote(self.postgres_password)
        return (
            f"postgresql+asyncpg://{user}:{password}"
            f"@{self.postgres_host}:{self.postgres_port}/{self.postgres_db}"
        )

    # ---- Redis URL ----
    @property
    def redis_url(self) -> str:
        return f"redis://{self.redis_host}:{self.redis_port}/0"


def get_settings() -> Settings:
    return Settings()
