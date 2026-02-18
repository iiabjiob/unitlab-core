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

    # ---- Sequences ----
    sequence_command_stream: str = "sequence:commands"
    sequence_command_dlq_stream: str = "sequence:commands:dlq"
    sequence_event_stream: str = "sequence:events"
    sequence_stream_maxlen: int = 5000

    # ---- Signal allocation jobs ----
    signal_allocation_job_stream: str = "signal-allocation:jobs"
    signal_allocation_job_stream_maxlen: int = 5000
    signal_test_run_job_stream: str = "signal-test-run:jobs"
    signal_test_run_job_stream_maxlen: int = 5000
    signal_allocation_job_ttl_seconds: int = 3600
    signal_test_run_max_signals: int = 20000
    signal_test_run_tested_at_batch_size: int = 50
    signal_test_run_ttl_refresh_seconds: int = 15
    signal_test_run_cancelling_stale_seconds: int = 90

    # ---- Redis ----
    redis_host: str
    redis_port: int
    ws_events_channel: str = "ws:events"

    # ---- Heartbeat ----
    heartbeat_ttl: int = 30
    check_heartbeat_interval: int = 5

    # ---- Worker Health ----
    worker_health_interval: int = 10
    worker_health_ttl: int = 30

    # ---- Signal Import ----
    signal_import_max_rows: int = 20000

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
