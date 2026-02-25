from __future__ import annotations

import asyncio
from contextlib import suppress
import logging
import re
from dataclasses import replace
from typing import Any

from .config import AgentConfig
from .models import AccessPointInfo, CommandEnvelope, CoreNetworkSnapshot, StaInfo, WifiNetwork
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
            wifi_iface=config.wifi_interface,
            mac=None,
            suffix=None,
        )

    async def start(self) -> None:
        await self.redis.ensure_group()
        await self._initialize_ap_identity()
        await self._enter_ap_mode(reason="boot")
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
    def _normalize_suffix(raw_suffix: str | None) -> str:
        if not raw_suffix:
            return "0000"
        pairs = re.findall(r"[0-9A-Fa-f]{2}", raw_suffix)
        if len(pairs) >= 2:
            return "".join(pairs[-2:]).upper()
        hex_only = "".join(ch for ch in raw_suffix if ch.lower() in "0123456789abcdef")
        return hex_only[-4:].upper().rjust(4, "0")

    async def _initialize_ap_identity(self) -> None:
        self._mac = await self.nmcli.get_mac()
        raw_suffix = await self.nmcli.get_mac_suffix()
        self._suffix = self._normalize_suffix(raw_suffix)
        if raw_suffix != self._suffix:
            logger.warning("Normalized MAC suffix | raw=%s normalized=%s", raw_suffix, self._suffix)
        self._ap_ssid = f"{self.config.ap_ssid_prefix}-{self._suffix}"
        self._ap_password = f"{self.config.ap_password_prefix}{self._suffix}"
        self._snapshot.ap.ssid = self._ap_ssid
        self._snapshot.ap.password = self._ap_password
        self._snapshot.mac = self._mac
        self._snapshot.suffix = self._suffix

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
            else:
                await self.redis.publish_event(
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
            await self.redis.publish_event(
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
                await self.redis.ack(cmd.entry_id)

    async def _handle_scan(self, cmd: CommandEnvelope) -> None:
        await self.redis.publish_event(
            "scan_started",
            {"request_id": cmd.request_id, "action": cmd.action},
        )
        networks = await self.nmcli.scan_networks()
        self._snapshot.available_networks = networks
        await self._refresh_runtime_status(publish=True, request_id=cmd.request_id, last_event="scan_result")
        await self.redis.publish_event(
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
        await self.redis.publish_event(
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
            await self.redis.publish_event(
                "sta_connect_failed",
                {"request_id": cmd.request_id, "ssid": ssid, "error": err},
            )
            await self._enter_ap_mode(reason="sta_connect_failed", request_id=cmd.request_id, error=err)
        else:
            await self.redis.publish_event(
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

    async def _enter_ap_mode(self, *, reason: str, request_id: str | None = None, error: str | None = None) -> None:
        if not self._ap_ssid or not self._ap_password:
            await self._initialize_ap_identity()
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
        await self._publish_snapshot(last_event="ap_active", request_id=request_id)
        await self.redis.publish_event(
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
        await self._publish_snapshot(last_event="sta_connected", request_id=request_id)

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
        if publish:
            await self._publish_snapshot(last_event=last_event, request_id=request_id)

    async def _publish_snapshot(
        self,
        *,
        last_event: str | None = None,
        request_id: str | None = None,
    ) -> None:
        self._snapshot.last_event = last_event or self._snapshot.last_event
        snapshot = self._snapshot.to_dict()
        if request_id:
            snapshot["request_id"] = request_id
        await self.redis.set_state(snapshot)
        await self.redis.publish_event("state", snapshot)

    async def _publish_error(self, event_type: str, error: str) -> None:
        self._snapshot.mode = "error"
        self._snapshot.last_error = error
        await self._publish_snapshot(last_event=event_type)
        await self.redis.publish_event(event_type, {"error": error})

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
