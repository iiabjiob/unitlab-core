from __future__ import annotations

import asyncio

from unitlab_rpi_net_agent.config import load_config
from unitlab_rpi_net_agent.nmcli_adapter import NmcliAdapter


def _adapter() -> NmcliAdapter:
    return NmcliAdapter(load_config())


def test_device_statuses_reads_ipv4_and_type_from_device_details() -> None:
    adapter = _adapter()

    async def fake_run(*args: str, **_kwargs: object) -> str:
        if args[-2:] == ("device", "status"):
            return "eth0:ethernet:connected (externally):unitlab-lan\nwlan0:wifi:connected:unitlab-ap"
        if args[-3:] == ("device", "show", "eth0"):
            return "GENERAL.TYPE:ethernet\nIP4.ADDRESS[1]:192.168.248.10/24"
        if args[-3:] == ("device", "show", "wlan0"):
            return "GENERAL.TYPE:wifi\nIP4.ADDRESS[1]:10.42.0.1/24"
        raise AssertionError(args)

    adapter._run = fake_run  # type: ignore[method-assign]
    statuses = asyncio.run(adapter.device_statuses())

    assert [(item.interface_name, item.device_type, item.ip4, item.ip4_prefix) for item in statuses] == [
        ("eth0", "ethernet", "192.168.248.10", 24),
        ("wlan0", "wifi", "10.42.0.1", 24),
    ]


def test_device_status_reads_type_and_ipv4() -> None:
    adapter = _adapter()

    async def fake_run(*_args: str, **_kwargs: object) -> str:
        return "GENERAL.TYPE:ethernet\nGENERAL.STATE:100 (connected)\nGENERAL.CONNECTION:unitlab-lan\nIP4.ADDRESS[1]:192.168.248.10/24"

    adapter._run = fake_run  # type: ignore[method-assign]
    status = asyncio.run(adapter.device_status("eth0"))

    assert status.device_type == "ethernet"
    assert status.ip4 == "192.168.248.10"
    assert status.ip4_prefix == 24
