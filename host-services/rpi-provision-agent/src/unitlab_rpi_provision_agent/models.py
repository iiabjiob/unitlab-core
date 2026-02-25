from __future__ import annotations

from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from typing import Any, Literal


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


ProvisionMode = Literal["ok", "degraded", "error", "unknown"]


@dataclass
class ProvisionCheck:
    key: str
    label: str
    ok: bool | None
    detail: str | None = None


@dataclass
class ProvisionActionResult:
    action: str
    success: bool
    message: str
    exit_code: int | None = None
    duration_ms: int | None = None


@dataclass
class CoreProvisionSnapshot:
    mode: ProvisionMode
    project_root: str
    checks: list[ProvisionCheck]
    smoke_checks: list[ProvisionCheck]
    last_action_result: ProvisionActionResult | None = None
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

