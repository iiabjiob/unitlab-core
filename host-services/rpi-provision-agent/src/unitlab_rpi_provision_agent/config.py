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
    status_publish_interval_sec: int
    command_timeout_sec: int
    systemctl_bin: str
    curl_bin: str
    docker_bin: str
    project_root: str
    log_level: str


def load_config() -> AgentConfig:
    host = socket.gethostname().strip() or "unitlab-core"
    pid = os.getpid()
    return AgentConfig(
      redis_url=os.getenv("UNITLAB_PROVISION_AGENT_REDIS_URL", "redis://127.0.0.1:6379/0"),
      redis_command_stream=os.getenv("UNITLAB_PROVISION_AGENT_COMMAND_STREAM", "core_provision:commands"),
      redis_event_stream=os.getenv("UNITLAB_PROVISION_AGENT_EVENT_STREAM", "core_provision:events"),
      redis_state_key=os.getenv("UNITLAB_PROVISION_AGENT_STATE_KEY", "core_provision:state"),
      redis_consumer_group=os.getenv("UNITLAB_PROVISION_AGENT_CONSUMER_GROUP", "core-provision-agent"),
      redis_consumer_name=os.getenv("UNITLAB_PROVISION_AGENT_CONSUMER_NAME", f"{host}-{pid}"),
      redis_stream_maxlen=max(100, _env_int("UNITLAB_PROVISION_AGENT_STREAM_MAXLEN", 2000)),
      command_block_ms=max(100, _env_int("UNITLAB_PROVISION_AGENT_COMMAND_BLOCK_MS", 5000)),
      status_publish_interval_sec=max(3, _env_int("UNITLAB_PROVISION_AGENT_STATUS_PUBLISH_INTERVAL_SEC", 15)),
      command_timeout_sec=max(2, _env_int("UNITLAB_PROVISION_AGENT_COMMAND_TIMEOUT_SEC", 30)),
      systemctl_bin=os.getenv("UNITLAB_PROVISION_AGENT_SYSTEMCTL_BIN", "systemctl"),
      curl_bin=os.getenv("UNITLAB_PROVISION_AGENT_CURL_BIN", "curl"),
      docker_bin=os.getenv("UNITLAB_PROVISION_AGENT_DOCKER_BIN", "docker"),
      project_root=os.getenv("UNITLAB_PROVISION_AGENT_PROJECT_ROOT", "/opt/unitlab/unitlab-core"),
      log_level=os.getenv("UNITLAB_PROVISION_AGENT_LOG_LEVEL", "INFO"),
    )

