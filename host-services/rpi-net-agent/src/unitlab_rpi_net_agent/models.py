from __future__ import annotations

from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from typing import Any, Literal


NetMode = Literal["unknown", "ap", "sta", "switching", "error"]
StaState = Literal["disconnected", "connecting", "connected", "failed"]


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


@dataclass
class AccessPointInfo:
    ssid: str
    password: str
    profile: str
    iface: str
    ip: str | None
    active: bool


@dataclass
class StaInfo:
    state: StaState
    ssid: str | None = None
    profile: str | None = None
    ip: str | None = None
    last_error: str | None = None


@dataclass
class WifiNetwork:
    ssid: str
    signal: int | None
    security: str | None
    in_use: bool = False


@dataclass
class NetworkInterfaceInfo:
    interface_name: str
    device_type: str | None
    local_ip: str | None
    netmask: str | None
    network: str | None
    connection: str | None = None
    state: str | None = None


@dataclass
class HostNetworkSettings:
    interface: str
    profile: str
    ipv4_mode: Literal["auto", "manual"]
    address_cidr: str | None
    gateway: str | None
    dns_servers: list[str]
    proxy_url: str | None
    proxy_no_proxy: list[str]
    last_applied_at: str | None = None
    last_error: str | None = None


@dataclass
class CoreNetworkSnapshot:
    mode: NetMode
    ap: AccessPointInfo
    sta: StaInfo
    host_network: HostNetworkSettings
    wifi_iface: str
    mac: str | None
    suffix: str | None
    interfaces: list[NetworkInterfaceInfo] = field(default_factory=list)
    request_in_flight: dict[str, Any] | None = None
    last_event: str | None = None
    last_error: str | None = None
    available_networks: list[WifiNetwork] = field(default_factory=list)
    updated_at: str = field(default_factory=utc_now_iso)

    def to_dict(self) -> dict[str, Any]:
        payload = asdict(self)
        payload["updated_at"] = utc_now_iso()
        return payload


@dataclass(frozen=True)
class CommandEnvelope:
    entry_id: str
    request_id: str
    action: str
    payload: dict[str, Any]
