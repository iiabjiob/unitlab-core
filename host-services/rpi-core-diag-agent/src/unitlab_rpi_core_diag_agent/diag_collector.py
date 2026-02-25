from __future__ import annotations

import asyncio
import os
import platform
import socket
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from .config import AgentConfig
from .models import (
    CoreDiagSnapshot,
    CpuDiagnostics,
    DiskDiagnostics,
    HostServiceStatus,
    MemoryDiagnostics,
)


class CoreDiagError(RuntimeError):
    pass


@dataclass(frozen=True)
class CollectedDiagnostics:
    snapshot: CoreDiagSnapshot


class DiagnosticsCollector:
    def __init__(self, config: AgentConfig) -> None:
        self.config = config

    async def _run(self, *args: str, timeout: int | None = None, check: bool = False) -> tuple[int, str, str]:
        proc = await asyncio.create_subprocess_exec(
            *args,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        try:
            stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=timeout or self.config.command_timeout_sec)
        except asyncio.TimeoutError as exc:
            proc.kill()
            raise CoreDiagError(f"Command timeout: {' '.join(args)}") from exc
        out = stdout.decode("utf-8", errors="ignore").strip()
        err = stderr.decode("utf-8", errors="ignore").strip()
        if check and proc.returncode != 0:
            raise CoreDiagError(f"Command failed ({proc.returncode}): {' '.join(args)} :: {err or out}")
        return proc.returncode, out, err

    async def collect(self) -> CollectedDiagnostics:
        time_utc = None
        try:
            import datetime as _dt
            time_utc = _dt.datetime.now(_dt.timezone.utc).isoformat()
        except Exception:
            time_utc = None

        cpu = CpuDiagnostics(
            temperature_c=_read_cpu_temp_c(),
            load_1m=None,
            load_5m=None,
            load_15m=None,
        )
        try:
            load1, load5, load15 = os.getloadavg()
            cpu.load_1m = float(load1)
            cpu.load_5m = float(load5)
            cpu.load_15m = float(load15)
        except (OSError, ValueError):
            pass

        memory = _read_meminfo()
        disk_root = _read_disk_usage("/")
        services = await self._read_services((
            "docker",
            "NetworkManager",
            "chrony",
            "unitlab-rpi-net-agent",
            "unitlab-rpi-ntp-agent",
            "unitlab-rpi-core-diag-agent",
        ))

        mode = "ok"
        if cpu.temperature_c is not None and cpu.temperature_c >= 80.0:
            mode = "degraded"
        if memory.used_percent is not None and memory.used_percent >= 95.0:
            mode = "degraded"
        if disk_root.used_percent is not None and disk_root.used_percent >= 95.0:
            mode = "degraded"
        if any(s.active is False and s.name in {"docker", "NetworkManager"} for s in services):
            mode = "error"

        snapshot = CoreDiagSnapshot(
            mode=mode,  # type: ignore[arg-type]
            hostname=socket.gethostname(),
            model=_read_model(),
            os_pretty_name=_read_os_pretty_name(),
            kernel=platform.release(),
            time_utc=time_utc,
            uptime_seconds=_read_uptime_seconds(),
            cpu=cpu,
            memory=memory,
            disk_root=disk_root,
            services=services,
        )
        return CollectedDiagnostics(snapshot=snapshot)

    async def _read_services(self, names: Iterable[str]) -> list[HostServiceStatus]:
        result: list[HostServiceStatus] = []
        for name in names:
            code, out, _err = await self._run(self.config.systemctl_bin, "is-active", name)
            normalized = out.strip().lower()
            active: bool | None
            if normalized == "active":
                active = True
            elif normalized in {"inactive", "failed", "activating", "deactivating"}:
                active = False
            else:
                active = None if code != 0 else False
            result.append(HostServiceStatus(name=name, active=active))
        return result


def _read_text_file(path: str) -> str | None:
    p = Path(path)
    if not p.exists():
        return None
    try:
        return p.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return None


def _read_model() -> str | None:
    value = _read_text_file("/proc/device-tree/model")
    if value is None:
        return None
    return value.replace("\x00", "").strip() or None


def _read_os_pretty_name() -> str | None:
    raw = _read_text_file("/etc/os-release")
    if raw is None:
        return None
    for line in raw.splitlines():
        if line.startswith("PRETTY_NAME="):
            value = line.split("=", 1)[1].strip().strip('"')
            return value or None
    return None


def _read_uptime_seconds() -> float | None:
    raw = _read_text_file("/proc/uptime")
    if raw is None:
        return None
    token = raw.strip().split()[0] if raw.strip() else ""
    try:
        return float(token)
    except ValueError:
        return None


def _read_cpu_temp_c() -> float | None:
    raw = _read_text_file("/sys/class/thermal/thermal_zone0/temp")
    if raw is None:
        return None
    try:
        milli = int(raw.strip())
        return milli / 1000.0
    except ValueError:
        return None


def _read_meminfo() -> MemoryDiagnostics:
    raw = _read_text_file("/proc/meminfo")
    if raw is None:
        return MemoryDiagnostics(None, None, None, None)
    kv: dict[str, int] = {}
    for line in raw.splitlines():
        if ":" not in line:
            continue
        key, rest = line.split(":", 1)
        token = rest.strip().split()[0] if rest.strip() else ""
        try:
            value_kib = int(token)
        except ValueError:
            continue
        kv[key.strip()] = value_kib * 1024
    total = kv.get("MemTotal")
    available = kv.get("MemAvailable")
    if total is None:
        return MemoryDiagnostics(None, None, None, None)
    used = total - available if available is not None else None
    used_percent = (float(used) / float(total) * 100.0) if used is not None and total > 0 else None
    return MemoryDiagnostics(total, available, used, used_percent)


def _read_disk_usage(path: str) -> DiskDiagnostics:
    try:
        st = os.statvfs(path)
        total = st.f_frsize * st.f_blocks
        free = st.f_frsize * st.f_bavail
        used = total - free
        used_percent = (float(used) / float(total) * 100.0) if total > 0 else None
        return DiskDiagnostics(path, total, free, used, used_percent)
    except OSError:
        return DiskDiagnostics(path, None, None, None, None)

