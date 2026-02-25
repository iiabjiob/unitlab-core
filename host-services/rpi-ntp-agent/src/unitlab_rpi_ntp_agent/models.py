from __future__ import annotations

from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from typing import Any, Literal


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


NtpMode = Literal["ok", "degraded", "error", "unknown"]


@dataclass
class ChronyTracking:
    synced: bool
    source: str | None = None
    stratum: int | None = None
    ref_time_utc: str | None = None
    system_time_offset_seconds: float | None = None
    last_offset_seconds: float | None = None
    rms_offset_seconds: float | None = None
    frequency_ppm: float | None = None
    residual_freq_ppm: float | None = None
    skew_ppm: float | None = None
    root_delay_seconds: float | None = None
    root_dispersion_seconds: float | None = None
    update_interval_seconds: float | None = None
    leap_status: str | None = None
    raw: dict[str, str] = field(default_factory=dict)


@dataclass
class ChronySource:
    mode_mark: str | None
    state_mark: str | None
    name: str
    stratum: int | None
    poll: int | None
    reach: int | None
    last_rx: str | None
    last_sample: str | None
    raw_line: str | None = None


@dataclass
class CoreNtpSnapshot:
    mode: NtpMode
    chrony_service_active: bool | None
    chrony_service_name: str | None
    configured_servers: list[str]
    effective_servers: list[str]
    tracking: ChronyTracking | None
    sources: list[ChronySource]
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

