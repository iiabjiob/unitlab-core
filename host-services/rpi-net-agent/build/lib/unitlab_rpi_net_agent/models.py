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
class CoreNetworkSnapshot:
    mode: NetMode
    ap: AccessPointInfo
    sta: StaInfo
    wifi_iface: str
    mac: str | None
    suffix: str | None
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

