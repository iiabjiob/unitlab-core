"""CLI entry point for the UnitLab MQTT device simulator."""

from __future__ import annotations

import argparse
import asyncio
import logging
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Iterable, Optional

import yaml

from simulator.devices.ao_device import SimulatedAODevice
from simulator.devices.di_device import SimulatedDIDevice
from simulator.devices.do_device import SimulatedDODevice
from simulator.mqtt_client import (
    BehaviorSettings,
    BrokerSettings,
    SimulatedDeviceBase,
    install_signal_handlers,
)
from simulator.utils.logger import get_logger, setup_logging

_DEFAULT_CONFIG = Path(__file__).with_name("config.yaml")
_PROFILE_FACTORS = {
    "default": 1.0,
    "load": 1.0,
    "smoke": 0.25,
    "stress": 2.0,
}

logger = get_logger("main")


@dataclass(slots=True)
class DeviceGroupConfig:
    count: int
    signals: int
    interval: float

    def scaled(self, factor: float) -> "DeviceGroupConfig":
        if factor == 1.0:
            return self
        new_count = int(round(self.count * factor))
        if self.count > 0:
            new_count = max(1, new_count)
        else:
            new_count = 0
        return DeviceGroupConfig(new_count, self.signals, self.interval)


@dataclass(slots=True)
class DevicesConfig:
    do: DeviceGroupConfig
    di: DeviceGroupConfig
    ao: DeviceGroupConfig

    def scaled(self, factor: float) -> "DevicesConfig":
        if factor == 1.0:
            return self
        return DevicesConfig(
            self.do.scaled(factor),
            self.di.scaled(factor),
            self.ao.scaled(factor),
        )


@dataclass(slots=True)
class SimulatorConfig:
    broker: BrokerSettings
    devices: DevicesConfig
    behavior: BehaviorSettings

    def scaled(self, factor: float) -> "SimulatorConfig":
        if factor == 1.0:
            return self
        return SimulatorConfig(
            broker=self.broker,
            devices=self.devices.scaled(factor),
            behavior=self.behavior,
        )


def parse_args(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="UnitLab MQTT simulator")
    parser.add_argument("--config", type=Path, default=_DEFAULT_CONFIG, help="Path to YAML config")
    parser.add_argument("--profile", choices=sorted(_PROFILE_FACTORS), default="default", help="Load profile name")
    parser.add_argument("--do", type=int, dest="do_count", help="Override DO device count")
    parser.add_argument("--di", type=int, dest="di_count", help="Override DI device count")
    parser.add_argument("--ao", type=int, dest="ao_count", help="Override AO device count")
    parser.add_argument("--hex-dump", action="store_true", help="Print hex dump for every published packet")
    parser.add_argument("--chaos", action="store_true", help="Enable aggressive reconnects and packet drops")
    parser.add_argument("--max-runtime", type=float, help="Stop after N seconds (useful for tests)")
    parser.add_argument("--verbose", action="store_true", help="Enable debug logging")
    return parser.parse_args(list(argv) if argv is not None else None)


def load_config(path: Path) -> SimulatorConfig:
    if not path.exists():
        raise FileNotFoundError(f"Configuration file not found: {path}")
    with path.open("r", encoding="utf-8") as fh:
        data = yaml.safe_load(fh) or {}
    return _parse_config_dict(data)


def _parse_config_dict(data: dict) -> SimulatorConfig:
    broker = data.get("broker") or {}
    devices = data.get("devices") or {}
    behavior = data.get("behavior") or {}

    broker_cfg = BrokerSettings(
        host=str(broker.get("host", "localhost")),
        port=int(broker.get("port", 1883)),
        keepalive=int(broker.get("keepalive", 30)),
        username=broker.get("username"),
        password=broker.get("password"),
    )

    def _device(section: str) -> DeviceGroupConfig:
        cfg = devices.get(section) or {}
        return DeviceGroupConfig(
            count=_ensure_int(cfg.get("count", 0), f"devices.{section}.count", min_value=0),
            signals=_ensure_int(cfg.get("signals", 1), f"devices.{section}.signals", min_value=1),
            interval=_ensure_float(cfg.get("interval", 1.0), f"devices.{section}.interval", min_value=0.05),
        )

    devices_cfg = DevicesConfig(
        do=_device("DO"),
        di=_device("DI"),
        ao=_device("AO"),
    )

    behavior_cfg = BehaviorSettings(
        randomize=bool(behavior.get("randomize", True)),
        heartbeat=_ensure_float(behavior.get("heartbeat", 10.0), "behavior.heartbeat", min_value=0.5),
        reconnect_chance=_ensure_float(behavior.get("reconnect_chance", 0.01), "behavior.reconnect_chance", min_value=0.0),
        packet_loss=_ensure_float(behavior.get("packet_loss", 0.0), "behavior.packet_loss", min_value=0.0, max_value=0.5),
    )

    return SimulatorConfig(broker=broker_cfg, devices=devices_cfg, behavior=behavior_cfg)


def _ensure_int(value, name: str, *, min_value: int = 0) -> int:
    if not isinstance(value, int):
        try:
            value = int(value)
        except (TypeError, ValueError) as exc:  # pragma: no cover - defensive
            raise ValueError(f"Expected integer for {name}") from exc
    if value < min_value:
        raise ValueError(f"{name} must be >= {min_value}")
    return value


def _ensure_float(value, name: str, *, min_value: float = 0.0, max_value: Optional[float] = None) -> float:
    try:
        value = float(value)
    except (TypeError, ValueError) as exc:  # pragma: no cover - defensive
        raise ValueError(f"Expected float for {name}") from exc
    if value < min_value:
        raise ValueError(f"{name} must be >= {min_value}")
    if max_value is not None and value > max_value:
        raise ValueError(f"{name} must be <= {max_value}")
    return value


async def run_simulator(args: argparse.Namespace) -> None:
    config = load_config(args.config)
    factor = _PROFILE_FACTORS.get(args.profile, 1.0)
    config = config.scaled(factor)

    if args.chaos:
        behavior = config.behavior
        config = replace(
            config,
            behavior=BehaviorSettings(
                randomize=True,
                heartbeat=max(1.0, behavior.heartbeat * 0.5),
                reconnect_chance=min(0.5, behavior.reconnect_chance * 5 + 0.05),
                packet_loss=min(0.2, behavior.packet_loss * 5 + 0.01),
            ),
        )

    devices_cfg = config.devices
    if args.do_count is not None:
        devices_cfg = replace(devices_cfg, do=replace(devices_cfg.do, count=max(0, args.do_count)))
    if args.di_count is not None:
        devices_cfg = replace(devices_cfg, di=replace(devices_cfg.di, count=max(0, args.di_count)))
    if args.ao_count is not None:
        devices_cfg = replace(devices_cfg, ao=replace(devices_cfg.ao, count=max(0, args.ao_count)))
    config = replace(config, devices=devices_cfg)

    devices: list[SimulatedDeviceBase] = []
    devices.extend(_build_do_devices(config, args.hex_dump))
    devices.extend(_build_di_devices(config, args.hex_dump))
    devices.extend(_build_ao_devices(config, args.hex_dump))

    if not devices:
        logger.warning("No devices configured – simulator will idle")

    stop_event = asyncio.Event()
    install_signal_handlers(stop_event)

    tasks = [asyncio.create_task(device.run(), name=f"sim:{device.unit_id}") for device in devices]

    async def _stop_after_timeout(timeout: float) -> None:
        try:
            await asyncio.sleep(timeout)
            logger.info("Max runtime reached (%.2fs)", timeout)
            stop_event.set()
        except asyncio.CancelledError:  # pragma: no cover - cooperative cancellation
            pass

    timeout_task: Optional[asyncio.Task[None]] = None
    if args.max_runtime:
        timeout_task = asyncio.create_task(_stop_after_timeout(args.max_runtime))

    try:
        await stop_event.wait()
    except asyncio.CancelledError:  # pragma: no cover - cooperative cancellation
        logger.info("Cancellation requested")
    finally:
        if timeout_task:
            timeout_task.cancel()
        await asyncio.gather(*(device.stop() for device in devices), return_exceptions=True)
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)


def _build_do_devices(config: SimulatorConfig, hex_dump: bool) -> list[SimulatedDeviceBase]:
    devices: list[SimulatedDeviceBase] = []
    group = config.devices.do
    width = max(3, len(str(max(group.count, 1))))
    for idx in range(group.count):
        unit_id = f"DO-{idx + 1:0{width}d}"
        devices.append(
            SimulatedDODevice(
                unit_id=unit_id,
                signals=group.signals,
                interval=group.interval,
                broker=config.broker,
                behavior=config.behavior,
                test_mode=hex_dump,
            )
        )
    return devices


def _build_di_devices(config: SimulatorConfig, hex_dump: bool) -> list[SimulatedDeviceBase]:
    devices: list[SimulatedDeviceBase] = []
    group = config.devices.di
    width = max(3, len(str(max(group.count, 1))))
    for idx in range(group.count):
        unit_id = f"DI-{idx + 1:0{width}d}"
        devices.append(
            SimulatedDIDevice(
                unit_id=unit_id,
                signals=group.signals,
                interval=group.interval,
                broker=config.broker,
                behavior=config.behavior,
                test_mode=hex_dump,
            )
        )
    return devices


def _build_ao_devices(config: SimulatorConfig, hex_dump: bool) -> list[SimulatedDeviceBase]:
    devices: list[SimulatedDeviceBase] = []
    group = config.devices.ao
    width = max(3, len(str(max(group.count, 1))))
    for idx in range(group.count):
        unit_id = f"AO-{idx + 1:0{width}d}"
        devices.append(
            SimulatedAODevice(
                unit_id=unit_id,
                signals=group.signals,
                interval=group.interval,
                broker=config.broker,
                behavior=config.behavior,
                test_mode=hex_dump,
            )
        )
    return devices


def run_cli(argv: Optional[Iterable[str]] = None) -> None:
    args = parse_args(argv)
    setup_logging(logging.DEBUG if args.verbose else logging.INFO)
    try:
        asyncio.run(run_simulator(args))
    except KeyboardInterrupt:  # pragma: no cover - user interrupt
        logger.info("Interrupted by user")


if __name__ == "__main__":
    run_cli()
