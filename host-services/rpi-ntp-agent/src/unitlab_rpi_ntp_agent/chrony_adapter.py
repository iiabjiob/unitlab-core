from __future__ import annotations

import asyncio
import ipaddress
import logging
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from .config import AgentConfig
from .models import ChronySource, ChronyTracking


logger = logging.getLogger("unitlab.ntp_agent.chrony")


class ChronyError(RuntimeError):
    pass


@dataclass(frozen=True)
class ChronyStatus:
    service_active: bool | None
    service_name: str | None
    configured_servers: list[str]
    effective_servers: list[str]
    tracking: ChronyTracking | None
    sources: list[ChronySource]


class ChronyAdapter:
    _SERVER_RE = re.compile(r"^[A-Za-z0-9._:-]+$")

    def __init__(self, config: AgentConfig) -> None:
        self.config = config

    async def _run(self, *args: str, timeout: int | None = None, check: bool = True) -> str:
        if self.config.dry_run:
            logger.info("[dry-run] %s", " ".join(args))
            return ""
        proc = await asyncio.create_subprocess_exec(
            *args,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        try:
            stdout, stderr = await asyncio.wait_for(
                proc.communicate(),
                timeout=timeout or self.config.command_timeout_sec,
            )
        except asyncio.TimeoutError as exc:
            proc.kill()
            raise ChronyError(f"Command timeout: {' '.join(args)}") from exc
        out = stdout.decode("utf-8", errors="ignore").strip()
        err = stderr.decode("utf-8", errors="ignore").strip()
        if check and proc.returncode != 0:
            raise ChronyError(f"Command failed ({proc.returncode}): {' '.join(args)} :: {err or out}")
        return out

    async def get_status(self) -> ChronyStatus:
        service_active, service_name = await self._detect_chrony_service()
        configured_servers = self.read_configured_servers()
        tracking = await self._read_tracking()
        sources = await self._read_sources()
        effective_servers = [src.name for src in sources if src.name]
        return ChronyStatus(
            service_active=service_active,
            service_name=service_name,
            configured_servers=configured_servers,
            effective_servers=effective_servers,
            tracking=tracking,
            sources=sources,
        )

    async def _detect_chrony_service(self) -> tuple[bool | None, str | None]:
        for service_name in ("chrony", "chronyd"):
            out = await self._run(self.config.systemctl_bin, "is-active", service_name, check=False)
            normalized = out.strip().lower()
            if normalized == "active":
                return True, service_name
            if normalized in {"inactive", "failed", "activating", "deactivating"}:
                return False, service_name
        return None, None

    async def apply_servers(self, servers: list[str]) -> list[str]:
        normalized = self.normalize_servers(servers)
        await self._write_source_file(normalized)
        await self.reload_sources()
        return normalized

    async def restore_defaults(self) -> list[str]:
        defaults = list(self.config.default_servers)
        return await self.apply_servers(defaults)

    async def reload_sources(self) -> None:
        await self._run(self.config.chronyc_bin, "reload", "sources")

    def normalize_servers(self, servers: Iterable[str]) -> list[str]:
        result: list[str] = []
        seen: set[str] = set()
        for item in servers:
            value = str(item).strip()
            if not value:
                continue
            if not self._SERVER_RE.match(value):
                raise ChronyError(f"Invalid server value: {value}")
            # Validate IPs if it looks like an IP; otherwise allow hostname token.
            try:
                ipaddress.ip_address(value)
            except ValueError:
                pass
            if value in seen:
                continue
            seen.add(value)
            result.append(value)
        return result

    def read_configured_servers(self) -> list[str]:
        path = Path(self.config.chrony_source_file)
        if not path.exists():
            return []
        servers: list[str] = []
        for raw_line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            tokens = line.split()
            if len(tokens) >= 2 and tokens[0] in {"server", "pool"}:
                servers.append(tokens[1])
        return servers

    async def _write_source_file(self, servers: list[str]) -> None:
        path = Path(self.config.chrony_source_file)
        path.parent.mkdir(parents=True, exist_ok=True)
        content_lines = [
            "# Managed by UnitLab NTP agent",
            "# Do not edit manually unless host-service is disabled",
        ]
        content_lines.extend([f"server {server} iburst" for server in servers])
        content = "\n".join(content_lines) + "\n"
        if self.config.dry_run:
            logger.info("[dry-run] write %s\n%s", path, content)
            return
        tmp_path = path.with_suffix(path.suffix + ".tmp")
        tmp_path.write_text(content, encoding="utf-8")
        os.replace(tmp_path, path)

    async def _read_tracking(self) -> ChronyTracking | None:
        output = await self._run(self.config.chronyc_bin, "-n", "tracking", check=False)
        if not output:
            return None
        kv: dict[str, str] = {}
        for line in output.splitlines():
            if ":" not in line:
                continue
            key, value = line.split(":", 1)
            kv[key.strip()] = value.strip()
        leap_status = kv.get("Leap status")
        synced = bool(leap_status and leap_status.lower() == "normal")
        return ChronyTracking(
            synced=synced,
            source=kv.get("Reference ID") or kv.get("Ref ID"),
            stratum=_parse_int(kv.get("Stratum")),
            ref_time_utc=kv.get("Ref time (UTC)"),
            system_time_offset_seconds=_parse_float_head(kv.get("System time")),
            last_offset_seconds=_parse_float_head(kv.get("Last offset")),
            rms_offset_seconds=_parse_float_head(kv.get("RMS offset")),
            frequency_ppm=_parse_float_head(kv.get("Frequency")),
            residual_freq_ppm=_parse_float_head(kv.get("Residual freq")),
            skew_ppm=_parse_float_head(kv.get("Skew")),
            root_delay_seconds=_parse_float_head(kv.get("Root delay")),
            root_dispersion_seconds=_parse_float_head(kv.get("Root dispersion")),
            update_interval_seconds=_parse_float_head(kv.get("Update interval")),
            leap_status=leap_status,
            raw=kv,
        )

    async def _read_sources(self) -> list[ChronySource]:
        output = await self._run(self.config.chronyc_bin, "-n", "sources", check=False)
        sources: list[ChronySource] = []
        for raw_line in output.splitlines():
            line = raw_line.rstrip()
            if not line or line.startswith("MS Name/IP address") or line.startswith("=") or line.startswith("^") is False and line[0] not in {"^", "=", "#", "?"}:
                # skip headers/legends; data rows usually start with marks like ^*
                continue
            if len(line) < 2:
                continue
            mode_mark = line[0]
            state_mark = line[1] if len(line) > 1 else None
            rest = line[2:].strip()
            cols = rest.split()
            if len(cols) < 1:
                continue
            name = cols[0]
            stratum = _parse_int(cols[1]) if len(cols) > 1 else None
            poll = _parse_int(cols[2]) if len(cols) > 2 else None
            reach = _parse_int(cols[3]) if len(cols) > 3 else None
            last_rx = cols[4] if len(cols) > 4 else None
            last_sample = " ".join(cols[5:]) if len(cols) > 5 else None
            sources.append(
                ChronySource(
                    mode_mark=mode_mark,
                    state_mark=state_mark,
                    name=name,
                    stratum=stratum,
                    poll=poll,
                    reach=reach,
                    last_rx=last_rx,
                    last_sample=last_sample,
                    raw_line=line,
                )
            )
        return sources


def _parse_int(value: str | None) -> int | None:
    if value is None:
        return None
    token = value.strip().split()[0]
    try:
        return int(token)
    except ValueError:
        return None


def _parse_float_head(value: str | None) -> float | None:
    if value is None:
        return None
    token = value.strip().split()[0]
    token = token.replace("seconds", "").replace("ppm", "").strip()
    token = token.lstrip("+-") if token.startswith("+-") else token
    try:
        return float(token)
    except ValueError:
        return None

