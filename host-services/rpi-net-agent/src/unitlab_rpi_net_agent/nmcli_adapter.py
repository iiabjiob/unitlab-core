from __future__ import annotations

import asyncio
import hashlib
import logging
import re
import shutil
from dataclasses import dataclass
from pathlib import Path

from .config import AgentConfig
from .models import AddressProbeResult, WifiNetwork


logger = logging.getLogger("unitlab.net_agent.nmcli")


class NmcliError(RuntimeError):
    pass


@dataclass(frozen=True)
class DeviceStatus:
    interface_name: str
    device_type: str | None
    state_code: str | None
    state_text: str | None
    connection: str | None
    ip4: str | None
    ip4_prefix: int | None = None
    ip4_cidr: str | None = None
    carrier: bool | None = None
    oper_state: str | None = None
    is_default_route: bool = False
    default_route_metric: int | None = None


class NmcliAdapter:
    def __init__(self, config: AgentConfig) -> None:
        self.config = config

    async def _run(self, *args: str, timeout: int | None = None, check: bool = True) -> str:
        cmd = ("nmcli", *args)
        if self.config.dry_run:
            logger.info("[dry-run] %s", " ".join(cmd))
            return ""
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        try:
            stdout, stderr = await asyncio.wait_for(
                proc.communicate(),
                timeout=timeout or self.config.nmcli_timeout_sec,
            )
        except asyncio.TimeoutError as exc:
            proc.kill()
            raise NmcliError(f"Command timeout: {' '.join(cmd)}") from exc
        out = stdout.decode("utf-8", errors="ignore").strip()
        err = stderr.decode("utf-8", errors="ignore").strip()
        if check and proc.returncode != 0:
            raise NmcliError(f"nmcli failed ({proc.returncode}): {' '.join(cmd)} :: {err or out}")
        return out

    async def _run_external(self, *args: str, timeout: int, check: bool = False) -> tuple[int, str, str]:
        if self.config.dry_run:
            logger.info("[dry-run] %s", " ".join(args))
            return 1, "", "dry-run"
        proc = await asyncio.create_subprocess_exec(
            *args,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        try:
            stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=timeout)
        except asyncio.TimeoutError:
            proc.kill()
            return 124, "", "timeout"
        out = stdout.decode("utf-8", errors="ignore").strip()
        err = stderr.decode("utf-8", errors="ignore").strip()
        if check and proc.returncode != 0:
            raise NmcliError(f"command failed ({proc.returncode}): {' '.join(args)} :: {err or out}")
        return proc.returncode, out, err

    @staticmethod
    def _read_text(path: Path) -> str | None:
        try:
            if not path.exists():
                return None
            return path.read_text(encoding="utf-8", errors="ignore").strip() or None
        except Exception:  # noqa: BLE001
            return None

    def _read_interface_carrier(self, interface_name: str) -> bool | None:
        raw = self._read_text(Path("/sys/class/net") / interface_name / "carrier")
        if raw == "1":
            return True
        if raw == "0":
            return False
        return None

    def _read_interface_operstate(self, interface_name: str) -> str | None:
        return self._read_text(Path("/sys/class/net") / interface_name / "operstate")

    def _read_default_route_metrics(self) -> dict[str, int]:
        routes: dict[str, int] = {}
        raw = self._read_text(Path("/proc/net/route"))
        if not raw:
            return routes
        for line in raw.splitlines()[1:]:
            parts = line.split()
            if len(parts) < 8:
                continue
            iface, destination, _gateway, _flags, _refcnt, _use, metric_raw, _mask = parts[:8]
            if destination != "00000000":
                continue
            try:
                metric = int(metric_raw, 10)
            except ValueError:
                metric = 0
            current = routes.get(iface)
            if current is None or metric < current:
                routes[iface] = metric
        return routes

    async def wifi_radio_on(self) -> None:
        await self._run("radio", "wifi", "on")

    async def get_mac(self) -> str | None:
        out = await self._run("-g", "GENERAL.HWADDR", "device", "show", self.config.wifi_interface, check=False)
        mac = out.strip()
        if not mac or mac == "--":
            return None
        return mac

    async def get_mac_suffix(self) -> str:
        mac = await self.get_mac()
        if not mac:
            return "0000"
        pairs = re.findall(r"[0-9A-Fa-f]{2}", mac)
        if len(pairs) >= 2:
            return "".join(pairs[-2:]).upper()
        hex_only = "".join(ch for ch in mac if ch.lower() in "0123456789abcdef")
        return hex_only[-4:].upper().rjust(4, "0")

    async def connection_exists(self, profile_name: str) -> bool:
        out = await self._run("-t", "-f", "NAME", "connection", "show", check=False)
        names = [line.strip() for line in out.splitlines() if line.strip()]
        return profile_name in names

    async def ensure_ap_profile(self, *, ssid: str, password: str) -> None:
        profile = self.config.ap_profile_name
        iface = self.config.wifi_interface
        if not await self.connection_exists(profile):
            await self._run(
                "connection",
                "add",
                "type",
                "wifi",
                "ifname",
                iface,
                "con-name",
                profile,
                "ssid",
                ssid,
            )

        await self._run(
            "connection",
            "modify",
            profile,
            "connection.autoconnect",
            "yes",
            "connection.autoconnect-priority",
            "100",
            "802-11-wireless.mode",
            "ap",
            "802-11-wireless.band",
            self.config.ap_band,
            "802-11-wireless.channel",
            str(self.config.ap_channel),
            "802-11-wireless.ssid",
            ssid,
            "802-11-wireless-security.key-mgmt",
            "wpa-psk",
            "802-11-wireless-security.proto",
            "rsn",
            "802-11-wireless-security.group",
            "ccmp",
            "802-11-wireless-security.pairwise",
            "ccmp",
            "802-11-wireless-security.pmf",
            "0",
            "802-11-wireless-security.psk",
            password,
            "ipv4.method",
            "shared",
            "ipv4.addresses",
            self.config.ap_ip_cidr,
            "ipv6.method",
            "ignore",
        )

    async def ensure_ethernet_profile(
        self,
        *,
        profile: str,
        iface: str,
        ipv4_method: str,
        address_cidr: str | None,
        gateway: str | None,
        dns_servers: list[str],
    ) -> None:
        if not await self.connection_exists(profile):
            await self._run(
                "connection",
                "add",
                "type",
                "ethernet",
                "ifname",
                iface,
                "con-name",
                profile,
            )

        cmd: list[str] = [
            "connection",
            "modify",
            profile,
            "connection.autoconnect",
            "yes",
            "connection.interface-name",
            iface,
            "ipv6.method",
            "ignore",
        ]
        if ipv4_method == "manual":
            if not address_cidr:
                raise NmcliError("address_cidr is required for manual IPv4 configuration")
            cmd.extend([
                "ipv4.method",
                "manual",
                "ipv4.addresses",
                address_cidr,
                "ipv4.gateway",
                gateway or "",
                "ipv4.dns",
                ",".join(dns_servers),
                "ipv4.ignore-auto-dns",
                "yes",
            ])
        else:
            cmd.extend([
                "ipv4.method",
                "auto",
                "ipv4.addresses",
                "",
                "ipv4.gateway",
                "",
                "ipv4.dns",
                "",
                "ipv4.ignore-auto-dns",
                "no",
            ])
        await self._run(*cmd)

    async def activate_connection(self, profile_name: str) -> None:
        await self._run("connection", "up", profile_name)

    async def deactivate_connection(self, profile_name: str) -> None:
        await self._run("connection", "down", profile_name, check=False)

    async def disconnect_device(self) -> None:
        await self._run("device", "disconnect", self.config.wifi_interface, check=False)

    async def scan_networks(self) -> list[WifiNetwork]:
        iface = self.config.wifi_interface
        out = await self._run(
            "-t",
            "-f",
            "IN-USE,SSID,SIGNAL,SECURITY",
            "device",
            "wifi",
            "list",
            "ifname",
            iface,
            "--rescan",
            "yes",
            check=False,
            timeout=max(self.config.nmcli_timeout_sec, 30),
        )
        networks: list[WifiNetwork] = []
        seen: set[tuple[str, str | None]] = set()
        for line in out.splitlines():
            parts = line.split(":")
            if len(parts) < 4:
                continue
            in_use_raw, ssid, signal_raw = parts[0], parts[1], parts[2]
            security = ":".join(parts[3:]).strip() or None
            ssid = ssid.strip()
            if not ssid:
                continue
            try:
                signal = int(signal_raw.strip()) if signal_raw.strip() else None
            except ValueError:
                signal = None
            key = (ssid, security)
            if key in seen:
                continue
            seen.add(key)
            networks.append(
                WifiNetwork(
                    ssid=ssid,
                    signal=signal,
                    security=security,
                    in_use=in_use_raw.strip() == "*",
                )
            )
        networks.sort(key=lambda n: (-(n.signal or -1), n.ssid.lower()))
        return networks

    async def probe_ipv4_address(self, *, iface: str, address: str, timeout_sec: int = 1) -> AddressProbeResult:
        timeout = max(1, timeout_sec)
        if shutil.which("ping"):
            code, _out, err = await self._run_external(
                "ping",
                "-c",
                "1",
                "-W",
                str(timeout),
                "-I",
                iface,
                address,
                timeout=timeout + 1,
            )
            return AddressProbeResult(
                address=address,
                reachable=code == 0,
                method="ping",
                error=None if code in {0, 1} else (err or f"exit {code}"),
            )

        if shutil.which("arping"):
            code, _out, err = await self._run_external(
                "arping",
                "-c",
                "1",
                "-w",
                str(timeout),
                "-I",
                iface,
                address,
                timeout=timeout + 1,
            )
            return AddressProbeResult(
                address=address,
                reachable=code == 0,
                method="arping",
                error=None if code in {0, 1} else (err or f"exit {code}"),
            )

        return AddressProbeResult(
            address=address,
            reachable=None,
            method="unavailable",
            error="Neither ping nor arping is available on the host",
        )

    def _sta_profile_name(self, ssid: str) -> str:
        suffix = hashlib.sha1(ssid.encode("utf-8")).hexdigest()[:8]
        return f"{self.config.sta_profile_prefix}-{suffix}"

    async def connect_sta(self, *, ssid: str, password: str | None, hidden: bool = False) -> str:
        iface = self.config.wifi_interface
        profile = self._sta_profile_name(ssid)
        await self.deactivate_connection(self.config.ap_profile_name)
        await self.disconnect_device()
        # Remove profile to avoid stale credentials/policies.
        await self._run("connection", "delete", profile, check=False)

        cmd = ["device", "wifi", "connect", ssid, "ifname", iface, "name", profile]
        if password:
            cmd.extend(["password", password])
        if hidden:
            cmd.extend(["hidden", "yes"])
        await self._run(*cmd, timeout=max(self.config.sta_connect_timeout_sec, self.config.nmcli_timeout_sec))
        await self._run(
            "connection",
            "modify",
            profile,
            "connection.autoconnect",
            "no",
            "connection.autoconnect-priority",
            "-100",
            "ipv6.method",
            "ignore",
        )
        return profile

    async def device_statuses(self) -> list[DeviceStatus]:
        out = await self._run(
            "-t",
            "-f",
            "DEVICE,TYPE,STATE,CONNECTION,IP4.ADDRESS[1]",
            "device",
            "status",
            check=False,
        )
        default_route_metrics = self._read_default_route_metrics()
        statuses: list[DeviceStatus] = []
        for line in out.splitlines():
            parts = line.split(":", 4)
            if len(parts) < 3:
                continue
            device_name = parts[0].strip()
            device_type = parts[1].strip() or None
            state_raw = parts[2].strip() if len(parts) > 2 else ""
            connection = parts[3].strip() if len(parts) > 3 else None
            ip_cidr = parts[4].strip() if len(parts) > 4 else None
            if not device_name:
                continue
            if connection == "--":
                connection = None
            if ip_cidr == "--":
                ip_cidr = None
            state_code = None
            state_text = None
            if state_raw:
                if " " in state_raw:
                    state_code, state_text = state_raw.split(" ", 1)
                    state_text = state_text.strip("() ").strip()
                else:
                    state_text = state_raw
                    state_code = state_raw
            ip4 = None
            ip4_prefix = None
            if ip_cidr:
                if "/" in ip_cidr:
                    ip4, prefix_raw = ip_cidr.split("/", 1)
                    ip4 = ip4.strip() or None
                    try:
                        ip4_prefix = int(prefix_raw.strip())
                    except ValueError:
                        ip4_prefix = None
                else:
                    ip4 = ip_cidr.strip() or None
            statuses.append(
                DeviceStatus(
                    interface_name=device_name,
                    device_type=device_type,
                    state_code=state_code,
                    state_text=state_text,
                    connection=connection,
                    ip4=ip4,
                    ip4_prefix=ip4_prefix,
                    ip4_cidr=ip_cidr,
                    carrier=self._read_interface_carrier(device_name),
                    oper_state=self._read_interface_operstate(device_name),
                    is_default_route=device_name in default_route_metrics,
                    default_route_metric=default_route_metrics.get(device_name),
                )
            )
        return statuses

    async def device_status(self, interface: str) -> DeviceStatus:
        out = await self._run(
            "-t",
            "-f",
            "GENERAL.STATE,GENERAL.CONNECTION,IP4.ADDRESS[1]",
            "device",
            "show",
            interface,
            check=False,
        )
        kv: dict[str, str] = {}
        for line in out.splitlines():
            if ":" not in line:
                continue
            key, value = line.split(":", 1)
            kv[key.strip()] = value.strip()
        state_raw = kv.get("GENERAL.STATE")
        state_code = None
        state_text = None
        if state_raw:
            if " " in state_raw:
                state_code, state_text = state_raw.split(" ", 1)
                state_text = state_text.strip("() ").strip()
            else:
                state_code = state_raw
        connection = kv.get("GENERAL.CONNECTION")
        if connection == "--":
            connection = None
        ip_cidr = kv.get("IP4.ADDRESS[1]")
        if ip_cidr == "--":
            ip_cidr = None
        ip4 = None
        ip4_prefix = None
        if ip_cidr:
            if "/" in ip_cidr:
                ip4, prefix_raw = ip_cidr.split("/", 1)
                ip4 = ip4.strip() or None
                try:
                    ip4_prefix = int(prefix_raw.strip())
                except ValueError:
                    ip4_prefix = None
            else:
                ip4 = ip_cidr.strip() or None
        default_route_metrics = self._read_default_route_metrics()
        return DeviceStatus(
            interface_name=interface,
            device_type=None,
            state_code=state_code,
            state_text=state_text,
            connection=connection,
            ip4=ip4,
            ip4_prefix=ip4_prefix,
            ip4_cidr=ip_cidr,
            carrier=self._read_interface_carrier(interface),
            oper_state=self._read_interface_operstate(interface),
            is_default_route=interface in default_route_metrics,
            default_route_metric=default_route_metrics.get(interface),
        )

    async def current_device_status(self) -> DeviceStatus:
        return await self.device_status(self.config.wifi_interface)

    async def wait_for_sta_connected(self, expected_profile: str, timeout_sec: int) -> DeviceStatus:
        deadline = asyncio.get_event_loop().time() + timeout_sec
        last_status = await self.current_device_status()
        while asyncio.get_event_loop().time() < deadline:
            status = await self.current_device_status()
            last_status = status
            if status.connection == expected_profile and status.ip4:
                return status
            await asyncio.sleep(1)
        raise NmcliError(
            f"STA connection timeout for profile={expected_profile}; "
            f"last_state={last_status.state_code}/{last_status.state_text}, conn={last_status.connection}, ip={last_status.ip4}"
        )
