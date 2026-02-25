from __future__ import annotations

import os
import socket
from dataclasses import dataclass


def _env_bool(name: str, default: bool) -> bool:
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


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

    wifi_interface: str
    ap_profile_name: str
    sta_profile_prefix: str
    ap_ssid_prefix: str
    ap_password_prefix: str
    ap_ip_cidr: str
    ap_channel: int
    ap_band: str
    sta_connect_timeout_sec: int
    status_publish_interval_sec: int
    status_scan_interval_sec: int
    nmcli_timeout_sec: int

    log_level: str
    dry_run: bool


def load_config() -> AgentConfig:
    host = socket.gethostname().strip() or "unitlab-core"
    pid = os.getpid()
    return AgentConfig(
        redis_url=os.getenv("UNITLAB_NET_AGENT_REDIS_URL", "redis://127.0.0.1:6379/0"),
        redis_command_stream=os.getenv("UNITLAB_NET_AGENT_COMMAND_STREAM", "core_net:commands"),
        redis_event_stream=os.getenv("UNITLAB_NET_AGENT_EVENT_STREAM", "core_net:events"),
        redis_state_key=os.getenv("UNITLAB_NET_AGENT_STATE_KEY", "core_net:state"),
        redis_consumer_group=os.getenv("UNITLAB_NET_AGENT_CONSUMER_GROUP", "core-net-agent"),
        redis_consumer_name=os.getenv("UNITLAB_NET_AGENT_CONSUMER_NAME", f"{host}-{pid}"),
        redis_stream_maxlen=max(100, _env_int("UNITLAB_NET_AGENT_STREAM_MAXLEN", 2000)),
        command_block_ms=max(100, _env_int("UNITLAB_NET_AGENT_COMMAND_BLOCK_MS", 5000)),
        wifi_interface=os.getenv("UNITLAB_NET_AGENT_WIFI_IFACE", "wlan0"),
        ap_profile_name=os.getenv("UNITLAB_NET_AGENT_AP_PROFILE", "unitlab-ap"),
        sta_profile_prefix=os.getenv("UNITLAB_NET_AGENT_STA_PROFILE_PREFIX", "unitlab-sta"),
        ap_ssid_prefix=os.getenv("UNITLAB_NET_AGENT_AP_SSID_PREFIX", "[unitlab]-core"),
        ap_password_prefix=os.getenv("UNITLAB_NET_AGENT_AP_PASSWORD_PREFIX", "pwd!"),
        ap_ip_cidr=os.getenv("UNITLAB_NET_AGENT_AP_IP_CIDR", "10.42.0.1/24"),
        ap_channel=max(1, _env_int("UNITLAB_NET_AGENT_AP_CHANNEL", 6)),
        ap_band=os.getenv("UNITLAB_NET_AGENT_AP_BAND", "bg"),
        sta_connect_timeout_sec=max(5, _env_int("UNITLAB_NET_AGENT_STA_CONNECT_TIMEOUT_SEC", 35)),
        status_publish_interval_sec=max(2, _env_int("UNITLAB_NET_AGENT_STATUS_PUBLISH_INTERVAL_SEC", 5)),
        status_scan_interval_sec=max(5, _env_int("UNITLAB_NET_AGENT_STATUS_SCAN_INTERVAL_SEC", 5)),
        nmcli_timeout_sec=max(2, _env_int("UNITLAB_NET_AGENT_NMCLI_TIMEOUT_SEC", 20)),
        log_level=os.getenv("UNITLAB_NET_AGENT_LOG_LEVEL", "INFO"),
        dry_run=_env_bool("UNITLAB_NET_AGENT_DRY_RUN", False),
    )
