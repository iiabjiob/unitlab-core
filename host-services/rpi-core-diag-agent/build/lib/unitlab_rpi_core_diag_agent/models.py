from __future__ import annotations

from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from typing import Any, Literal


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


DiagMode = Literal["ok", "degraded", "error", "unknown"]


@dataclass
class HostServiceStatus:
    name: str
    active: bool | None


@dataclass
class CpuDiagnostics:
    temperature_c: float | None
    load_1m: float | None
    load_5m: float | None
    load_15m: float | None


@dataclass
class MemoryDiagnostics:
    total_bytes: int | None
    available_bytes: int | None
    used_bytes: int | None
    used_percent: float | None


@dataclass
class DiskDiagnostics:
    mountpoint: str
    total_bytes: int | None
    free_bytes: int | None
    used_bytes: int | None
    used_percent: float | None


@dataclass
class CoreDiagSnapshot:
    mode: DiagMode
    hostname: str | None
    model: str | None
    os_pretty_name: str | None
    kernel: str | None
    time_utc: str | None
    uptime_seconds: float | None
    cpu: CpuDiagnostics
    memory: MemoryDiagnostics
    disk_root: DiskDiagnostics
    services: list[HostServiceStatus]
    request_in_flight: dict[str, Any] | None = None
    last_event: str | None = None
    last_error: str | None = None
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

