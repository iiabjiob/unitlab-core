# pyright: reportPrivateUsage=false
# pyright: reportUnknownParameterType=false
# pyright: reportMissingParameterType=false
# pyright: reportUnusedCallResult=false
from __future__ import annotations

import asyncio

import pytest
from unitlab_rpi_net_agent.config import load_config
from unitlab_rpi_net_agent.nmcli_adapter import NmcliAdapter, NmcliError


def _adapter() -> NmcliAdapter:
    return NmcliAdapter(load_config())


def test_device_statuses_reads_ipv4_and_type_from_device_details() -> None:
    adapter = _adapter()

    async def fake_run(*args: str, **_kwargs: object) -> str:
        if args[-2:] == ("device", "status"):
            return "eth0:ethernet:connected (externally):unitlab-lan\nwlan0:wifi:connected:unitlab-ap"
        if args[-3:] == ("device", "show", "eth0"):
            assert args[2] == "GENERAL.TYPE,IP4.ADDRESS"
            assert _kwargs["check"] is True
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
        assert _args[2] == "GENERAL.TYPE,GENERAL.STATE,GENERAL.CONNECTION,IP4.ADDRESS"
        assert _kwargs["check"] is True
        return "GENERAL.TYPE:ethernet\nGENERAL.STATE:100 (connected)\nGENERAL.CONNECTION:unitlab-lan\nIP4.ADDRESS[1]:192.168.248.10/24"

    adapter._run = fake_run  # type: ignore[method-assign]
    status = asyncio.run(adapter.device_status("eth0"))

    assert status.device_type == "ethernet"
    assert status.ip4 == "192.168.248.10"
    assert status.ip4_prefix == 24


def test_device_status_without_address_is_valid() -> None:
    adapter = _adapter()

    async def fake_run(*_args: str, **_kwargs: object) -> str:
        return "GENERAL.TYPE:ethernet\nGENERAL.STATE:30 (disconnected)\nGENERAL.CONNECTION:--"

    adapter._run = fake_run  # type: ignore[method-assign]
    status = asyncio.run(adapter.device_status("eth0"))
    assert status.ip4 is None
    assert status.connection is None
    assert status.state_text == "disconnected"


def test_device_details_failure_is_not_reported_as_missing_ipv4() -> None:
    adapter = _adapter()

    async def fake_run(*args: str, **kwargs: object) -> str:
        if args[-2:] == ("device", "status"):
            return "eth0:ethernet:connected:unitlab-lan"
        assert kwargs["check"] is True
        raise NmcliError("nmcli failed")

    adapter._run = fake_run  # type: ignore[method-assign]
    with pytest.raises(NmcliError, match="nmcli failed"):
        asyncio.run(adapter.device_statuses())


def test_device_statuses_excludes_container_and_loopback_interfaces() -> None:
    adapter = _adapter()

    async def fake_run(*args: str, **_kwargs: object) -> str:
        if args[-2:] == ("device", "status"):
            return "eth0:ethernet:connected:unitlab-lan\nbr-abc:bridge:connected:br-abc\nveth123:ethernet:connected:veth123\nlo:loopback:connected:lo"
        if args[-3:] == ("device", "show", "eth0"):
            return "GENERAL.TYPE:ethernet\nIP4.ADDRESS[1]:192.168.248.10/24"
        raise AssertionError(args)

    adapter._run = fake_run  # type: ignore[method-assign]
    statuses = asyncio.run(adapter.device_statuses())

    assert [item.interface_name for item in statuses] == ["eth0"]
