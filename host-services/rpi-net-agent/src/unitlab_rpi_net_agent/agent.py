from __future__ import annotations

import asyncio
import ipaddress
import json
from contextlib import suppress
import logging
import re
from dataclasses import asdict, replace
from pathlib import Path
from typing import Any

from .config import AgentConfig
from .models import (
    AccessPointInfo,
    AddressProbeSnapshot,
    CommandEnvelope,
    CoreNetworkSnapshot,
    HostNetworkSettings,
    NetworkInterfaceInfo,
    StaInfo,
    WifiNetwork,
)
from .nmcli_adapter import DeviceStatus, NmcliAdapter, NmcliError
from .redis_protocol import RedisProtocol


logger = logging.getLogger("unitlab.net_agent")


class CoreNetworkAgent:
    def __init__(self, config: AgentConfig) -> None:
        self.config = config
        self.nmcli = NmcliAdapter(config)
        self.redis = RedisProtocol(config)
        self._stop = asyncio.Event()
        self._status_task: asyncio.Task[None] | None = None
        self._command_task: asyncio.Task[None] | None = None
        self._ap_ssid: str | None = None
        self._ap_password: str | None = None
        self._mac: str | None = None
        self._suffix: str | None = None
        self._snapshot = CoreNetworkSnapshot(
            mode="unknown",
            ap=AccessPointInfo(
                ssid="",
                password="",
                profile=config.ap_profile_name,
                iface=config.wifi_interface,
                ip=None,
                active=False,
            ),
            sta=StaInfo(state="disconnected"),
            host_network=self._default_host_network_settings(),
            wifi_iface=config.wifi_interface,
            mac=None,
            suffix=None,
        )
        self._load_host_network_settings_best_effort()

    async def start(self) -> None:
        await self._initialize_ap_identity()
        reused_existing_ap = await self._restore_existing_ap_mode()
        if not reused_existing_ap:
            await self._enter_ap_mode(reason="boot")
        await self._refresh_interface_snapshots()
        await self._ensure_redis_group_best_effort("startup")
        self._status_task = asyncio.create_task(self._status_loop(), name="unitlab-net-agent-status")
        self._command_task = asyncio.create_task(self._command_loop(), name="unitlab-net-agent-commands")
        logger.info(
            "UnitLab RPi net-agent started | iface=%s ap_ssid=%s consumer=%s",
            self.config.wifi_interface,
            self._ap_ssid,
            self.config.redis_consumer_name,
        )

    async def stop(self) -> None:
        self._stop.set()
        for task in (self._status_task, self._command_task):
            if task:
                task.cancel()
        for task in (self._status_task, self._command_task):
            if task:
                with suppress(asyncio.CancelledError):
                    await task
        await self.redis.close()

    async def run_forever(self) -> None:
        await self.start()
        try:
            await self._stop.wait()
        finally:
            await self.stop()

    @staticmethod
    def _normalize_suffix(raw: str | None) -> str:
        if not raw:
            return "0000"
        pairs = re.findall(r"[0-9A-Fa-f]{2}", raw)
        if len(pairs) >= 2:
            return (pairs[-2] + pairs[-1]).upper()
        hex_only = "".join(ch for ch in raw if ch.lower() in "0123456789abcdef")
        return hex_only[-4:].upper().rjust(4, "0")

    @staticmethod
    def _sanitize_ap_prefix(prefix: str | None, fallback: str) -> str:
        value = (prefix or fallback).strip()
        value = value.replace("\\", "")
        return value or fallback

    @staticmethod
    def _sanitize_text(value: Any) -> str | None:
        if value is None:
            return None
        text = str(value).strip()
        return text or None

    @staticmethod
    def _normalize_text_list(value: Any) -> list[str]:
        if value is None:
            return []
        items: list[str] = []
        if isinstance(value, str):
            value = [part.strip() for part in value.split(",")]
        if isinstance(value, list):
            for item in value:
                text = str(item).strip()
                if text:
                    items.append(text)
        return items

    def _default_host_network_settings(self) -> HostNetworkSettings:
        mode = (self.config.ethernet_default_mode or "auto").strip().lower()
        if mode not in {"auto", "manual"}:
            mode = "auto"
        address_cidr = self.config.ethernet_default_address_cidr if mode == "manual" else None
        gateway = self.config.ethernet_default_gateway if mode == "manual" else None
        return HostNetworkSettings(
            interface=self.config.ethernet_interface,
            profile=self.config.ethernet_profile_name,
            ipv4_mode=mode,
            address_cidr=address_cidr,
            gateway=gateway,
            dns_servers=list(self.config.ethernet_default_dns_servers),
            proxy_url=self.config.proxy_url,
            proxy_no_proxy=list(self.config.proxy_no_proxy),
            last_applied_at=None,
            last_error=None,
        )

    def _load_host_network_settings_best_effort(self) -> None:
        path = Path(self.config.host_network_settings_file)
        if not path.exists():
            return
        try:
            raw = json.loads(path.read_text(encoding="utf-8", errors="ignore"))
        except Exception as exc:  # noqa: BLE001
            logger.warning("Failed to read host network settings file | path=%s error=%s", path, exc)
            return
        if not isinstance(raw, dict):
            return
        self._snapshot.host_network = self._host_network_settings_from_mapping(raw)

    def _host_network_settings_from_mapping(self, raw: dict[str, Any]) -> HostNetworkSettings:
        mode = self._sanitize_text(raw.get("ipv4_mode")) or self._snapshot.host_network.ipv4_mode
        mode = mode if mode in {"auto", "manual"} else self._snapshot.host_network.ipv4_mode
        return HostNetworkSettings(
            interface=self._sanitize_text(raw.get("interface")) or self.config.ethernet_interface,
            profile=self._sanitize_text(raw.get("profile")) or self.config.ethernet_profile_name,
            ipv4_mode=mode,
            address_cidr=self._sanitize_text(raw.get("address_cidr")),
            gateway=self._sanitize_text(raw.get("gateway")),
            dns_servers=self._normalize_text_list(raw.get("dns_servers")),
            proxy_url=self._sanitize_text(raw.get("proxy_url")),
            proxy_no_proxy=self._normalize_text_list(raw.get("proxy_no_proxy")),
            last_applied_at=self._sanitize_text(raw.get("last_applied_at")),
            last_error=self._sanitize_text(raw.get("last_error")),
        )

    async def _persist_host_network_settings(self) -> None:
        path = Path(self.config.host_network_settings_file)
        path.parent.mkdir(parents=True, exist_ok=True)
        payload = json.dumps(asdict(self._snapshot.host_network), indent=2, ensure_ascii=True)
        if self.config.dry_run:
            logger.info("[dry-run] write host network settings %s\n%s", path, payload)
            return
        tmp_path = path.with_suffix(path.suffix + ".tmp")
        tmp_path.write_text(payload + "\n", encoding="utf-8")
        tmp_path.replace(path)

    async def _write_proxy_environment_file(self, settings: HostNetworkSettings) -> None:
        path = Path(self.config.proxy_environment_file)
        if not settings.proxy_url and not settings.proxy_no_proxy:
            if self.config.dry_run:
                logger.info("[dry-run] clear proxy environment file %s", path)
                return
            if path.exists():
                path.unlink()
            return
        content_lines = [
            "# Managed by UnitLab net agent",
        ]
        if settings.proxy_url:
            content_lines.extend([
                f"http_proxy={settings.proxy_url}",
                f"https_proxy={settings.proxy_url}",
                f"HTTP_PROXY={settings.proxy_url}",
                f"HTTPS_PROXY={settings.proxy_url}",
            ])
        if settings.proxy_no_proxy:
            no_proxy = ",".join(settings.proxy_no_proxy)
            content_lines.extend([
                f"no_proxy={no_proxy}",
                f"NO_PROXY={no_proxy}",
            ])
        content = "\n".join(content_lines) + "\n"
        if self.config.dry_run:
            logger.info("[dry-run] write proxy environment file %s\n%s", path, content)
            return
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp_path = path.with_suffix(path.suffix + ".tmp")
        tmp_path.write_text(content, encoding="utf-8")
        tmp_path.replace(path)

    async def _initialize_ap_identity(self) -> None:
        self._mac = await self.nmcli.get_mac()
        self._suffix = self._normalize_suffix(self._mac)
        raw_suffix = await self.nmcli.get_mac_suffix()
        normalized_from_raw = self._normalize_suffix(raw_suffix)
        if normalized_from_raw != self._suffix:
            logger.warning(
                "MAC suffix mismatch | mac=%r suffix_from_mac=%s raw_suffix=%r suffix_from_raw=%s",
                self._mac,
                self._suffix,
                raw_suffix,
                normalized_from_raw,
            )
        ssid_prefix = self._sanitize_ap_prefix(self.config.ap_ssid_prefix, "[unitlab]-core")
        password_prefix = self._sanitize_ap_prefix(self.config.ap_password_prefix, "pwd!")
        if ssid_prefix != self.config.ap_ssid_prefix:
            logger.warning("Sanitized AP SSID prefix | raw=%s sanitized=%s", self.config.ap_ssid_prefix, ssid_prefix)
        if password_prefix != self.config.ap_password_prefix:
            logger.warning(
                "Sanitized AP password prefix | raw=%s sanitized=%s",
                self.config.ap_password_prefix,
                password_prefix,
            )
        self._ap_ssid = f"{ssid_prefix}-{self._suffix}"
        self._ap_password = f"{password_prefix}{self._suffix}"
        self._snapshot.ap.ssid = self._ap_ssid
        self._snapshot.ap.password = self._ap_password
        self._snapshot.mac = self._mac
        self._snapshot.suffix = self._suffix

    async def _restore_existing_ap_mode(self) -> bool:
        try:
            status = await self.nmcli.current_device_status()
        except Exception as exc:  # noqa: BLE001
            logger.warning("Failed to inspect startup Wi-Fi state: %s", exc)
            return False

        if status.connection != self.config.ap_profile_name or not status.ip4:
            return False

        self._snapshot.mode = "ap"
        self._snapshot.ap = replace(
            self._snapshot.ap,
            ssid=self._ap_ssid or self._snapshot.ap.ssid,
            password=self._ap_password or self._snapshot.ap.password,
            profile=self.config.ap_profile_name,
            iface=self.config.wifi_interface,
            ip=status.ip4,
            active=True,
        )
        self._snapshot.sta = StaInfo(state="disconnected")
        self._snapshot.last_error = None
        logger.info(
            "Reusing active AP on startup | profile=%s ip=%s",
            self.config.ap_profile_name,
            status.ip4,
        )
        return True

    async def _ensure_redis_group_best_effort(self, context: str) -> bool:
        try:
            await self.redis.ensure_group()
        except Exception as exc:  # noqa: BLE001
            logger.warning("Redis group ensure failed during %s: %s", context, exc)
            return False
        return True

    async def _publish_event_best_effort(self, event_type: str, payload: dict[str, Any]) -> bool:
        try:
            await self.redis.publish_event(event_type, payload)
        except Exception as exc:  # noqa: BLE001
            logger.warning("Failed to publish Redis event | event=%s error=%s", event_type, exc)
            return False
        return True

    async def _ack_best_effort(self, entry_id: str) -> bool:
        try:
            await self.redis.ack(entry_id)
        except Exception as exc:  # noqa: BLE001
            logger.warning("Failed to ack Redis command | entry=%s error=%s", entry_id, exc)
            return False
        return True

    async def _status_loop(self) -> None:
        while not self._stop.is_set():
            try:
                await self._refresh_runtime_status(publish=True)
            except asyncio.CancelledError:
                raise
            except Exception as exc:  # noqa: BLE001
                logger.exception("Status loop failed: %s", exc)
                await self._publish_error("status_loop_error", str(exc))
            await asyncio.sleep(self.config.status_publish_interval_sec)

    async def _command_loop(self) -> None:
        while not self._stop.is_set():
            try:
                commands = await self.redis.read_commands()
                for cmd in commands:
                    await self._handle_command(cmd)
            except asyncio.CancelledError:
                raise
            except Exception as exc:  # noqa: BLE001
                logger.exception("Command loop failed: %s", exc)
                await self._ensure_redis_group_best_effort("command loop recovery")
                await self._publish_error("command_loop_error", str(exc))
                await asyncio.sleep(1)

    async def _handle_command(self, cmd: CommandEnvelope) -> None:
        logger.info("Command received | request=%s action=%s entry=%s", cmd.request_id, cmd.action, cmd.entry_id)
        await self._set_request_in_flight(cmd)
        should_ack = True
        try:
            action = cmd.action.strip().lower()
            if action == "status":
                await self._refresh_runtime_status(publish=True, request_id=cmd.request_id)
            elif action == "scan":
                await self._handle_scan(cmd)
            elif action == "connect_sta":
                await self._handle_connect_sta(cmd)
            elif action == "disconnect_sta":
                await self._handle_disconnect_sta(cmd)
            elif action == "restart_ap":
                await self._enter_ap_mode(reason="restart_ap", request_id=cmd.request_id)
            elif action == "apply_network_settings":
                await self._handle_apply_network_settings(cmd)
            elif action == "restore_network_settings":
                await self._handle_restore_network_settings(cmd)
            elif action == "probe_addresses":
                await self._handle_probe_addresses(cmd)
            else:
                await self._publish_event_best_effort(
                    "command_rejected",
                    {
                        "request_id": cmd.request_id,
                        "entry_id": cmd.entry_id,
                        "action": cmd.action,
                        "reason": f"Unknown action: {cmd.action}",
                    },
                )
        except Exception as exc:  # noqa: BLE001
            logger.exception("Command failed | request=%s action=%s", cmd.request_id, cmd.action)
            await self._publish_event_best_effort(
                "command_failed",
                {
                    "request_id": cmd.request_id,
                    "entry_id": cmd.entry_id,
                    "action": cmd.action,
                    "error": str(exc),
                },
            )
        finally:
            await self._clear_request_in_flight()
            if should_ack:
                await self._ack_best_effort(cmd.entry_id)

    async def _handle_scan(self, cmd: CommandEnvelope) -> None:
        await self._publish_event_best_effort(
            "scan_started",
            {"request_id": cmd.request_id, "action": cmd.action},
        )
        networks = await self.nmcli.scan_networks()
        self._snapshot.available_networks = networks
        await self._refresh_runtime_status(publish=True, request_id=cmd.request_id, last_event="scan_result")
        await self._publish_event_best_effort(
            "scan_result",
            {
                "request_id": cmd.request_id,
                "count": len(networks),
                "networks": [self._network_to_dict(item) for item in networks],
            },
        )

    async def _handle_connect_sta(self, cmd: CommandEnvelope) -> None:
        ssid = str(cmd.payload.get("ssid") or "").strip()
        password = cmd.payload.get("password")
        hidden = bool(cmd.payload.get("hidden") or False)
        timeout_sec = int(cmd.payload.get("timeout_sec") or self.config.sta_connect_timeout_sec)
        if not ssid:
            raise ValueError("ssid is required")
        if password is not None:
            password = str(password)

        self._snapshot.mode = "switching"
        self._snapshot.sta = StaInfo(state="connecting", ssid=ssid)
        await self._publish_snapshot(last_event="sta_connecting", request_id=cmd.request_id)
        await self._publish_event_best_effort(
            "sta_connecting",
            {"request_id": cmd.request_id, "ssid": ssid, "hidden": hidden},
        )

        profile_name: str | None = None
        try:
            profile_name = await self.nmcli.connect_sta(ssid=ssid, password=password, hidden=hidden)
            status = await self.nmcli.wait_for_sta_connected(profile_name, timeout_sec=timeout_sec)
            await self._apply_sta_connected(status=status, ssid=ssid, profile_name=profile_name, request_id=cmd.request_id)
        except Exception as exc:  # noqa: BLE001
            err = str(exc)
            logger.warning("STA connect failed | request=%s ssid=%s error=%s", cmd.request_id, ssid, err)
            self._snapshot.sta = StaInfo(state="failed", ssid=ssid, profile=profile_name, last_error=err)
            self._snapshot.last_error = err
            await self._publish_event_best_effort(
                "sta_connect_failed",
                {"request_id": cmd.request_id, "ssid": ssid, "error": err},
            )
            await self._enter_ap_mode(reason="sta_connect_failed", request_id=cmd.request_id, error=err)
        else:
            await self._publish_event_best_effort(
                "sta_connected",
                {
                    "request_id": cmd.request_id,
                    "ssid": ssid,
                    "profile": profile_name,
                    "ip": self._snapshot.sta.ip,
                },
            )

    async def _handle_disconnect_sta(self, cmd: CommandEnvelope) -> None:
        await self.nmcli.disconnect_device()
        await self._enter_ap_mode(reason="disconnect_sta", request_id=cmd.request_id)

    async def _handle_apply_network_settings(self, cmd: CommandEnvelope) -> None:
        settings = self._normalize_host_network_settings(cmd.payload)
        await self._apply_network_settings(cmd=cmd, settings=settings)

    async def _handle_restore_network_settings(self, cmd: CommandEnvelope) -> None:
        previous = self._snapshot.previous_host_network
        if previous is None:
            raise ValueError("No previous network configuration is available to restore")
        await self._apply_network_settings(cmd=cmd, settings=previous)

    async def _handle_probe_addresses(self, cmd: CommandEnvelope) -> None:
        iface = self._sanitize_text(cmd.payload.get("interface")) or self._snapshot.host_network.interface
        raw_addresses = cmd.payload.get("addresses")
        if not isinstance(raw_addresses, list) or not raw_addresses:
            raise ValueError("addresses is required")
        timeout_sec = int(cmd.payload.get("timeout_sec") or 1)
        addresses: list[str] = []
        seen: set[str] = set()
        for raw in raw_addresses:
            text = self._sanitize_text(raw)
            if not text:
                continue
            try:
                parsed = ipaddress.ip_address(text)
            except ValueError as exc:
                raise ValueError(f"Invalid IPv4 address: {text}") from exc
            if parsed.version != 4:
                raise ValueError(f"Only IPv4 address probes are supported: {text}")
            normalized = str(parsed)
            if normalized in seen:
                continue
            seen.add(normalized)
            addresses.append(normalized)
        if not addresses:
            raise ValueError("No valid IPv4 addresses provided")

        await self._publish_event_best_effort(
            "address_probe_started",
            {"request_id": cmd.request_id, "interface": iface, "count": len(addresses)},
        )
        results = [
            await self.nmcli.probe_ipv4_address(iface=iface, address=address, timeout_sec=timeout_sec)
            for address in addresses
        ]
        self._snapshot.last_address_probe = AddressProbeSnapshot(
            request_id=cmd.request_id,
            interface=iface,
            checked_at=self._current_timestamp(),
            results=results,
        )
        await self._publish_snapshot(last_event="address_probe_result", request_id=cmd.request_id)
        await self._publish_event_best_effort(
            "address_probe_result",
            {
                "request_id": cmd.request_id,
                "interface": iface,
                "results": [asdict(result) for result in results],
            },
        )

    async def _apply_network_settings(self, *, cmd: CommandEnvelope, settings: HostNetworkSettings) -> None:
        previous_settings = self._snapshot.host_network
        self._snapshot.previous_host_network = previous_settings
        self._snapshot.host_network = replace(settings, last_error=None)
        self._snapshot.host_network.last_applied_at = None
        await self._publish_snapshot(last_event="network_settings_applying", request_id=cmd.request_id)
        await self._publish_event_best_effort(
            "network_settings_applying",
            {
                "request_id": cmd.request_id,
                "interface": settings.interface,
                "profile": settings.profile,
                "ipv4_mode": settings.ipv4_mode,
                "previous": asdict(previous_settings),
            },
        )
        try:
            await self.nmcli.ensure_ethernet_profile(
                profile=settings.profile,
                iface=settings.interface,
                ipv4_method=settings.ipv4_mode,
                address_cidr=settings.address_cidr,
                gateway=settings.gateway,
                dns_servers=settings.dns_servers,
            )
            await self._write_proxy_environment_file(settings)
            await self.nmcli.activate_connection(settings.profile)
            status = await self.nmcli.device_status(settings.interface)
            await self._apply_host_network_settings(status=status, settings=settings, request_id=cmd.request_id)
        except Exception as exc:  # noqa: BLE001
            err = str(exc)
            logger.warning("Host network settings apply failed | request=%s error=%s", cmd.request_id, err)
            self._snapshot.host_network = replace(self._snapshot.host_network, last_error=err)
            self._snapshot.last_error = err
            await self._publish_snapshot(last_event="network_settings_failed", request_id=cmd.request_id)
            await self._publish_event_best_effort(
                "network_settings_failed",
                {
                    "request_id": cmd.request_id,
                    "error": err,
                    "interface": settings.interface,
                    "profile": settings.profile,
                },
            )
            raise

    def _normalize_host_network_settings(self, payload: dict[str, Any]) -> HostNetworkSettings:
        interface = self._sanitize_text(payload.get("interface")) or self.config.ethernet_interface
        profile = self._sanitize_text(payload.get("profile")) or self.config.ethernet_profile_name
        ipv4_mode = (self._sanitize_text(payload.get("ipv4_mode")) or self._snapshot.host_network.ipv4_mode).lower()
        if ipv4_mode not in {"auto", "manual"}:
            raise ValueError(f"Invalid ipv4_mode: {ipv4_mode}")
        address_cidr = self._sanitize_text(payload.get("address_cidr"))
        gateway = self._sanitize_text(payload.get("gateway"))
        dns_servers = self._normalize_text_list(payload.get("dns_servers"))
        proxy_url = self._sanitize_text(payload.get("proxy_url"))
        proxy_no_proxy = self._normalize_text_list(payload.get("proxy_no_proxy"))
        return HostNetworkSettings(
            interface=interface,
            profile=profile,
            ipv4_mode=ipv4_mode,
            address_cidr=address_cidr,
            gateway=gateway,
            dns_servers=dns_servers,
            proxy_url=proxy_url,
            proxy_no_proxy=proxy_no_proxy,
            last_applied_at=self._snapshot.host_network.last_applied_at,
            last_error=self._snapshot.host_network.last_error,
        )

    async def _apply_host_network_settings(
        self,
        *,
        status: DeviceStatus,
        settings: HostNetworkSettings,
        request_id: str | None,
    ) -> None:
        applied_at = self._current_timestamp()
        self._snapshot.host_network = replace(settings, last_applied_at=applied_at, last_error=None)
        self._snapshot.last_error = None
        await self._refresh_interface_snapshots()
        await self._persist_host_network_settings()
        await self._publish_snapshot(last_event="network_settings_applied", request_id=request_id)
        await self._publish_event_best_effort(
            "network_settings_applied",
            {
                "request_id": request_id,
                "interface": settings.interface,
                "profile": settings.profile,
                "ipv4_mode": settings.ipv4_mode,
                "ip": status.ip4,
                "ip_cidr": status.ip4_cidr,
            },
        )

    async def _enter_ap_mode(self, *, reason: str, request_id: str | None = None, error: str | None = None) -> None:
        if not self._ap_ssid or not self._ap_password:
            try:
                await self._initialize_ap_identity()
            except Exception as exc:  # noqa: BLE001
                logger.warning("Failed to init AP identity, using fallback | error=%s", exc)
                self._suffix = self._suffix or "0000"
                ssid_prefix = self._sanitize_ap_prefix(self.config.ap_ssid_prefix, "[unitlab]-core")
                password_prefix = self._sanitize_ap_prefix(self.config.ap_password_prefix, "pwd!")
                self._ap_ssid = f"{ssid_prefix}-{self._suffix}"
                self._ap_password = f"{password_prefix}{self._suffix}"
                self._snapshot.ap.ssid = self._ap_ssid
                self._snapshot.ap.password = self._ap_password
                self._snapshot.suffix = self._suffix
        assert self._ap_ssid is not None
        assert self._ap_password is not None

        await self.nmcli.wifi_radio_on()
        await self.nmcli.ensure_ap_profile(ssid=self._ap_ssid, password=self._ap_password)
        await self.nmcli.activate_connection(self.config.ap_profile_name)
        status = await self.nmcli.current_device_status()
        self._snapshot.mode = "ap"
        self._snapshot.ap = replace(
            self._snapshot.ap,
            ssid=self._ap_ssid,
            password=self._ap_password,
            profile=self.config.ap_profile_name,
            iface=self.config.wifi_interface,
            ip=status.ip4 or self._snapshot.ap.ip,
            active=True,
        )
        self._snapshot.sta = StaInfo(state="disconnected")
        self._snapshot.last_error = error
        await self._refresh_interface_snapshots()
        await self._publish_snapshot(last_event="ap_active", request_id=request_id)
        await self._publish_event_best_effort(
            "ap_active",
            {
                "request_id": request_id,
                "reason": reason,
                "ssid": self._snapshot.ap.ssid,
                "ip": self._snapshot.ap.ip,
                "iface": self.config.wifi_interface,
            },
        )

    async def _apply_sta_connected(
        self,
        *,
        status: DeviceStatus,
        ssid: str,
        profile_name: str,
        request_id: str | None,
    ) -> None:
        self._snapshot.mode = "sta"
        self._snapshot.ap = replace(self._snapshot.ap, active=False)
        self._snapshot.sta = StaInfo(
            state="connected",
            ssid=ssid,
            profile=profile_name,
            ip=status.ip4,
            last_error=None,
        )
        self._snapshot.last_error = None
        await self._refresh_interface_snapshots()
        await self._publish_snapshot(last_event="sta_connected", request_id=request_id)

    async def _safe_device_status(self, interface: str) -> DeviceStatus | None:
        try:
            return await self.nmcli.device_status(interface)
        except Exception as exc:  # noqa: BLE001
            logger.debug("Failed to read device status | interface=%s error=%s", interface, exc)
            return None

    def _build_interface_snapshots(self, statuses: list[DeviceStatus]) -> list[NetworkInterfaceInfo]:
        items: list[NetworkInterfaceInfo] = []
        seen: set[str] = set()
        for status in statuses:
            if status.interface_name in seen:
                continue
            seen.add(status.interface_name)
            local_ip = status.ip4
            netmask = str(status.ip4_prefix) if status.ip4_prefix is not None else None
            network = None
            if status.ip4_cidr:
                try:
                    network = str(ipaddress.ip_interface(status.ip4_cidr).network)
                except ValueError:
                    network = None
            items.append(
                NetworkInterfaceInfo(
                    interface_name=status.interface_name,
                    device_type=status.device_type,
                    local_ip=local_ip,
                    netmask=netmask,
                    network=network,
                    connection=status.connection,
                    state=status.state_text,
                    carrier=status.carrier,
                    oper_state=status.oper_state,
                    is_default_route=status.is_default_route,
                    default_route_metric=status.default_route_metric,
                )
            )
        return items

    async def _refresh_interface_snapshots(self) -> None:
        statuses: list[DeviceStatus] = []
        try:
            statuses = await self.nmcli.device_statuses()
        except Exception as exc:  # noqa: BLE001
            logger.debug("Failed to list host interfaces via nmcli: %s", exc)
        selected_interface = self._snapshot.host_network.interface or self.config.ethernet_interface
        if selected_interface and not any(item.interface_name == selected_interface for item in statuses):
            selected_status = await self._safe_device_status(selected_interface)
            if selected_status is not None:
                statuses.append(selected_status)
        wifi_interface = self.config.wifi_interface
        if wifi_interface and not any(item.interface_name == wifi_interface for item in statuses):
            wifi_status = await self._safe_device_status(wifi_interface)
            if wifi_status is not None:
                statuses.append(wifi_status)
        self._snapshot.interfaces = self._build_interface_snapshots(statuses=statuses)

    async def _refresh_runtime_status(
        self,
        *,
        publish: bool,
        request_id: str | None = None,
        last_event: str | None = None,
    ) -> None:
        status = await self.nmcli.current_device_status()
        previous_ap_ip = self._snapshot.ap.ip
        reported_ip = status.ip4
        profile_matches = status.connection == self.config.ap_profile_name
        was_ap_mode = self._snapshot.mode == "ap"
        ap_ip_matches = bool(previous_ap_ip and reported_ip and previous_ap_ip.split("/", 1)[0] == reported_ip.split("/", 1)[0])
        ap_fallback_active = (status.connection is None) and was_ap_mode and ap_ip_matches
        ap_active = profile_matches or ap_fallback_active
        self._snapshot.mac = self._mac or self._snapshot.mac
        self._snapshot.suffix = self._suffix or self._snapshot.suffix
        self._snapshot.ap = replace(
            self._snapshot.ap,
            active=ap_active,
            ip=reported_ip if ap_active else self._snapshot.ap.ip,
        )
        if status.connection and status.connection != self.config.ap_profile_name:
            self._snapshot.mode = "sta"
            self._snapshot.sta = StaInfo(
                state="connected" if status.ip4 else "connecting",
                ssid=self._snapshot.sta.ssid,
                profile=status.connection,
                ip=status.ip4,
                last_error=self._snapshot.sta.last_error,
            )
        elif ap_active:
            self._snapshot.mode = "ap"
            if self._snapshot.sta.state != "failed":
                self._snapshot.sta = StaInfo(state="disconnected")
        elif self._snapshot.mode not in {"switching", "error"}:
            self._snapshot.mode = "unknown"
        await self._refresh_interface_snapshots()
        if publish:
            await self._publish_snapshot(last_event=last_event, request_id=request_id)

    async def _publish_snapshot(
        self,
        *,
        last_event: str | None = None,
        request_id: str | None = None,
    ) -> bool:
        self._snapshot.last_event = last_event or self._snapshot.last_event
        snapshot = self._snapshot.to_dict()
        if request_id:
            snapshot["request_id"] = request_id
        try:
            await self.redis.set_state(snapshot)
            await self.redis.publish_event("state", snapshot)
        except Exception as exc:  # noqa: BLE001
            logger.warning(
                "Failed to publish snapshot to Redis | event=%s error=%s",
                self._snapshot.last_event,
                exc,
            )
            return False
        return True

    async def _publish_error(self, event_type: str, error: str) -> None:
        self._snapshot.mode = "error"
        self._snapshot.last_error = error
        await self._publish_snapshot(last_event=event_type)
        await self._publish_event_best_effort(event_type, {"error": error})

    async def _set_request_in_flight(self, cmd: CommandEnvelope) -> None:
        self._snapshot.request_in_flight = {
            "request_id": cmd.request_id,
            "entry_id": cmd.entry_id,
            "action": cmd.action,
        }
        await self._publish_snapshot(last_event="command_started", request_id=cmd.request_id)

    async def _clear_request_in_flight(self) -> None:
        self._snapshot.request_in_flight = None
        await self._publish_snapshot(last_event="command_finished")

    @staticmethod
    def _network_to_dict(item: WifiNetwork) -> dict[str, Any]:
        return {
            "ssid": item.ssid,
            "signal": item.signal,
            "security": item.security,
            "in_use": item.in_use,
        }

    @staticmethod
    def _current_timestamp() -> str:
        from datetime import datetime, timezone

        return datetime.now(timezone.utc).isoformat()
