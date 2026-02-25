from __future__ import annotations

import os
import socket
from dataclasses import dataclass


def _env_int(name: str, default: int) -> int:
    raw = os.getenv(name)
    if raw is None:
        return default
    try:
        return int(raw.strip())
    except ValueError:
        return default


def _env_bool(name: str, default: bool) -> bool:
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


@dataclass(frozen=True)
class AgentConfig:
    redis_url: str
    redis_command_stream: str
    redis_event_stream: str
    redis_state_key: str
    redis_consumer_group: str
    redis_consumer_name: str
    redis_stream_maxlen: int
    command_block_ms: int

    chrony_sources_dir: str
    chrony_source_file: str
    chronyc_bin: str
    systemctl_bin: str
    command_timeout_sec: int
    status_publish_interval_sec: int
    default_servers: tuple[str, ...]

    log_level: str
    dry_run: bool


def load_config() -> AgentConfig:
    host = socket.gethostname().strip() or "unitlab-core"
    pid = os.getpid()
    sources_dir = "/etc/chrony/sources.d"
    source_file = os.path.join(sources_dir, "unitlab-ntp.sources")
    default_servers = ("pool.ntp.org", "time.google.com")

    return AgentConfig(
        redis_url=os.getenv("UNITLAB_NTP_AGENT_REDIS_URL", "redis://127.0.0.1:6379/0"),
        redis_command_stream=os.getenv("UNITLAB_NTP_AGENT_COMMAND_STREAM", "core_ntp:commands"),
        redis_event_stream=os.getenv("UNITLAB_NTP_AGENT_EVENT_STREAM", "core_ntp:events"),
        redis_state_key=os.getenv("UNITLAB_NTP_AGENT_STATE_KEY", "core_ntp:state"),
        redis_consumer_group=os.getenv("UNITLAB_NTP_AGENT_CONSUMER_GROUP", "core-ntp-agent"),
        redis_consumer_name=os.getenv("UNITLAB_NTP_AGENT_CONSUMER_NAME", f"{host}-{pid}"),
        redis_stream_maxlen=max(100, _env_int("UNITLAB_NTP_AGENT_STREAM_MAXLEN", 2000)),
        command_block_ms=max(100, _env_int("UNITLAB_NTP_AGENT_COMMAND_BLOCK_MS", 5000)),
        chrony_sources_dir=sources_dir,
        chrony_source_file=source_file,
        chronyc_bin="chronyc",
        systemctl_bin="systemctl",
        command_timeout_sec=max(2, _env_int("UNITLAB_NTP_AGENT_COMMAND_TIMEOUT_SEC", 10)),
        status_publish_interval_sec=max(2, _env_int("UNITLAB_NTP_AGENT_STATUS_PUBLISH_INTERVAL_SEC", 10)),
        default_servers=default_servers,
        log_level=os.getenv("UNITLAB_NTP_AGENT_LOG_LEVEL", "INFO"),
        dry_run=_env_bool("UNITLAB_NTP_AGENT_DRY_RUN", False),
    )

