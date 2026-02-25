from __future__ import annotations

import asyncio
import os
import time
from pathlib import Path

from .config import AgentConfig
from .models import ProvisionActionResult, ProvisionCheck


class ProvisionOps:
    def __init__(self, config: AgentConfig) -> None:
        self.config = config

    async def _run(self, *args: str, cwd: str | None = None, timeout: int | None = None) -> tuple[int, str, str]:
        proc = await asyncio.create_subprocess_exec(
            *args,
            cwd=cwd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        try:
            stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=timeout or self.config.command_timeout_sec)
        except asyncio.TimeoutError:
            proc.kill()
            return 124, "", f"Command timeout: {' '.join(args)}"
        return proc.returncode, stdout.decode("utf-8", errors="ignore"), stderr.decode("utf-8", errors="ignore")

    async def collect_checks(self) -> list[ProvisionCheck]:
        project = Path(self.config.project_root)
        checks: list[ProvisionCheck] = []
        compose_path = project / "docker-compose.prod.yml"

        checks.append(ProvisionCheck("project_root_exists", "Project root exists", project.exists(), str(project)))
        checks.append(ProvisionCheck("compose_file", "docker-compose.prod.yml present", compose_path.exists(), str(compose_path)))
        checks.append(self._frontend_delivery_check(project, compose_path))
        checks.append(ProvisionCheck("backend_env_prod", "backend/.env.prod present", (project / "backend" / ".env.prod").exists(), str(project / "backend" / ".env.prod")))
        checks.append(ProvisionCheck("backend_env_db_prod", "backend/.env.db.prod present", (project / "backend" / ".env.db.prod").exists(), str(project / "backend" / ".env.db.prod")))

        code, out, _err = await self._run(self.config.docker_bin, "--version")
        checks.append(ProvisionCheck("docker_cli", "Docker CLI available", code == 0, out.strip() or None))

        code, out, err = await self._run(self.config.docker_bin, "compose", "version")
        checks.append(ProvisionCheck("docker_compose", "Docker Compose plugin available", code == 0, (out or err).strip() or None))

        for svc in ("docker", "NetworkManager", "chrony"):
          code, out, _err = await self._run(self.config.systemctl_bin, "is-active", svc)
          checks.append(ProvisionCheck(f"service_{svc}", f"systemd service {svc}", out.strip() == "active", out.strip() or None))

        for svc in ("unitlab-rpi-net-agent", "unitlab-rpi-ntp-agent", "unitlab-rpi-core-diag-agent"):
          code, out, _err = await self._run(self.config.systemctl_bin, "is-active", svc)
          active = out.strip() == "active"
          detail = out.strip() or ("missing" if code != 0 else None)
          checks.append(ProvisionCheck(f"host_agent_{svc}", f"host agent {svc}", active if code == 0 else False, detail))

        return checks

    def _frontend_delivery_check(self, project: Path, compose_path: Path) -> ProvisionCheck:
        frontend_dist = project / "frontend" / "dist"
        if frontend_dist.exists():
            return ProvisionCheck(
                "frontend_delivery",
                "Frontend delivery available (dist or image-based)",
                True,
                f"dist mode: {frontend_dist}",
            )

        if compose_path.exists():
            try:
                compose_text = compose_path.read_text(encoding="utf-8", errors="ignore")
            except OSError as exc:
                return ProvisionCheck("frontend_delivery", "Frontend delivery available (dist or image-based)", False, str(exc))

            if "UNITLAB_WEB_IMAGE" in compose_text:
                env_path = project / ".env"
                env_detail = None
                backend_detail = None
                if env_path.exists():
                    try:
                        for line in env_path.read_text(encoding="utf-8", errors="ignore").splitlines():
                            if line.startswith("UNITLAB_WEB_IMAGE="):
                                env_detail = line.strip()
                            elif line.startswith("UNITLAB_BACKEND_IMAGE="):
                                backend_detail = line.strip()
                    except OSError:
                        env_detail = None
                        backend_detail = None
                detail_parts = [
                    env_detail or "web image mode configured in docker-compose.prod.yml (UNITLAB_WEB_IMAGE)"
                ]
                if backend_detail:
                    detail_parts.append(backend_detail)
                detail = " | ".join(detail_parts)
                return ProvisionCheck("frontend_delivery", "Frontend delivery available (dist or image-based)", True, detail)

        return ProvisionCheck(
            "frontend_delivery",
            "Frontend delivery available (dist or image-based)",
            False,
            "Missing frontend/dist and compose does not declare UNITLAB_WEB_IMAGE image delivery",
        )

    async def run_smoke_checks(self) -> list[ProvisionCheck]:
        checks: list[ProvisionCheck] = []
        urls = [
            ("api_health", "API /health", "http://127.0.0.1/api/v1/health"),
            ("api_core_network", "API /core-network/state", "http://127.0.0.1/api/v1/core-network/state"),
            ("api_core_ntp", "API /core-ntp/state", "http://127.0.0.1/api/v1/core-ntp/state"),
            ("api_core_diag", "API /core-diagnostics/state", "http://127.0.0.1/api/v1/core-diagnostics/state"),
        ]
        for key, label, url in urls:
            code, out, err = await self._run(self.config.curl_bin, "-fsS", "-m", "3", url)
            checks.append(ProvisionCheck(key, label, code == 0, (out[:120] if code == 0 else err.strip() or out.strip() or None)))
        return checks

    async def run_install_action(self, action: str) -> ProvisionActionResult:
        script_map = {
            "install_net_agent": "host-services/rpi-net-agent/install/install_rpi_net_agent.sh",
            "install_ntp_agent": "host-services/rpi-ntp-agent/install/install_rpi_ntp_agent.sh",
            "install_diag_agent": "host-services/rpi-core-diag-agent/install/install_rpi_core_diag_agent.sh",
        }
        rel = script_map.get(action)
        if not rel:
            return ProvisionActionResult(action=action, success=False, message=f"Unsupported action: {action}")

        script = Path(self.config.project_root) / rel
        if not script.exists():
            return ProvisionActionResult(action=action, success=False, message=f"Script not found: {script}")

        started = time.monotonic()
        code, out, err = await self._run(str(script), timeout=max(self.config.command_timeout_sec, 900))
        duration_ms = int((time.monotonic() - started) * 1000)
        success = code == 0
        message = (out.strip() or err.strip() or f"exit_code={code}")[:1000]
        return ProvisionActionResult(
            action=action,
            success=success,
            message=message,
            exit_code=code,
            duration_ms=duration_ms,
        )
