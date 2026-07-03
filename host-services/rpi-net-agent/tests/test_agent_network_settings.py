from __future__ import annotations

import asyncio
from pathlib import Path

from unitlab_rpi_net_agent.agent import CoreNetworkAgent
from unitlab_rpi_net_agent.config import AgentConfig
from unitlab_rpi_net_agent.models import AddressProbeResult, CommandEnvelope, HostNetworkSettings
from unitlab_rpi_net_agent.nmcli_adapter import DeviceStatus


def _make_config(tmp_path: Path) -> AgentConfig:
    return AgentConfig(
        redis_url="redis://127.0.0.1:6379/0",
        redis_command_stream="core_net:commands",
        redis_event_stream="core_net:events",
        redis_state_key="core_net:state",
        redis_consumer_group="core-net-agent",
        redis_consumer_name="test-consumer",
        redis_stream_maxlen=2000,
        command_block_ms=100,
        wifi_interface="wlan0",
        ethernet_interface="eth0",
        ap_profile_name="unitlab-ap",
        ethernet_profile_name="unitlab-lan",
        sta_profile_prefix="unitlab-sta",
        ap_ssid_prefix="[unitlab]-core",
        ap_password_prefix="pwd!",
        ap_ip_cidr="10.42.0.1/24",
        ap_channel=6,
        ap_band="bg",
        ethernet_default_mode="manual",
        ethernet_default_address_cidr="192.168.10.21/24",
        ethernet_default_gateway="192.168.10.1",
        ethernet_default_dns_servers=["192.168.10.1"],
        proxy_url="http://proxy:3128",
        proxy_no_proxy=["localhost"],
        host_network_settings_file=str(tmp_path / "network.json"),
        proxy_environment_file=str(tmp_path / "proxy.conf"),
        sta_connect_timeout_sec=35,
        status_publish_interval_sec=5,
        status_scan_interval_sec=5,
        nmcli_timeout_sec=20,
        log_level="INFO",
        dry_run=True,
    )


def test_core_network_agent_exposes_host_network_defaults(tmp_path: Path) -> None:
    agent = CoreNetworkAgent(_make_config(tmp_path))

    assert agent._snapshot.host_network.interface == "eth0"
    assert agent._snapshot.host_network.profile == "unitlab-lan"
    assert agent._snapshot.host_network.ipv4_mode == "manual"
    assert agent._snapshot.host_network.address_cidr == "192.168.10.21/24"
    assert agent._snapshot.host_network.gateway == "192.168.10.1"
    assert agent._snapshot.host_network.dns_servers == ["192.168.10.1"]
    assert agent._snapshot.host_network.proxy_url == "http://proxy:3128"
    assert agent._snapshot.host_network.proxy_no_proxy == ["localhost"]


def test_core_network_agent_builds_interface_snapshots_from_device_statuses(tmp_path: Path) -> None:
    agent = CoreNetworkAgent(_make_config(tmp_path))

    snapshots = agent._build_interface_snapshots(
        [
            DeviceStatus(
                interface_name="eth0",
                device_type="ethernet",
                state_code="100",
                state_text="connected",
                connection="Wired connection 1",
                ip4="192.168.10.21",
                ip4_prefix=24,
                ip4_cidr="192.168.10.21/24",
                carrier=True,
                oper_state="up",
                is_default_route=True,
                default_route_metric=100,
            ),
            DeviceStatus(
                interface_name="wlan0",
                device_type="wifi",
                state_code="30",
                state_text="disconnected",
                connection=None,
                ip4=None,
                ip4_prefix=None,
                ip4_cidr=None,
                carrier=False,
                oper_state="down",
                is_default_route=False,
                default_route_metric=None,
            ),
        ]
    )

    assert [item.interface_name for item in snapshots] == ["eth0", "wlan0"]
    assert snapshots[0].device_type == "ethernet"
    assert snapshots[0].local_ip == "192.168.10.21"
    assert snapshots[0].netmask == "24"
    assert snapshots[0].network == "192.168.10.0/24"
    assert snapshots[0].carrier is True
    assert snapshots[0].oper_state == "up"
    assert snapshots[0].is_default_route is True
    assert snapshots[0].default_route_metric == 100
    assert snapshots[1].device_type == "wifi"
    assert snapshots[1].carrier is False


def test_core_network_agent_records_previous_network_settings_for_restore(tmp_path: Path) -> None:
    agent = CoreNetworkAgent(_make_config(tmp_path))
    current = agent._snapshot.host_network
    next_settings = HostNetworkSettings(
        interface="eth0",
        profile="unitlab-lan",
        ipv4_mode="manual",
        address_cidr="192.168.20.10/24",
        gateway=None,
        dns_servers=[],
        proxy_url=None,
        proxy_no_proxy=[],
    )

    class FakeNmcli:
        async def ensure_ethernet_profile(self, **_kwargs) -> None:
            return None

        async def activate_connection(self, _profile: str) -> None:
            return None

        async def device_status(self, interface: str) -> DeviceStatus:
            return DeviceStatus(
                interface_name=interface,
                device_type="ethernet",
                state_code="100",
                state_text="connected",
                connection="unitlab-lan",
                ip4="192.168.20.10",
                ip4_prefix=24,
                ip4_cidr="192.168.20.10/24",
            )

        async def device_statuses(self) -> list[DeviceStatus]:
            return []

    class FakeRedis:
        async def set_state(self, _snapshot) -> None:
            return None

        async def publish_event(self, _event_type, _payload) -> None:
            return None

    agent.nmcli = FakeNmcli()  # type: ignore[assignment]
    agent.redis = FakeRedis()  # type: ignore[assignment]

    asyncio.run(agent._apply_network_settings(
        cmd=CommandEnvelope(entry_id="1", request_id="req", action="apply_network_settings", payload={}),
        settings=next_settings,
    ))

    assert agent._snapshot.previous_host_network == current
    assert agent._snapshot.host_network.address_cidr == "192.168.20.10/24"


def test_core_network_agent_probes_addresses_through_adapter(tmp_path: Path) -> None:
    agent = CoreNetworkAgent(_make_config(tmp_path))

    class FakeNmcli:
        async def probe_ipv4_address(self, *, iface: str, address: str, timeout_sec: int) -> AddressProbeResult:
            return AddressProbeResult(address=address, reachable=address.endswith(".10"), method=f"fake:{iface}:{timeout_sec}")

    class FakeRedis:
        async def set_state(self, _snapshot) -> None:
            return None

        async def publish_event(self, _event_type, _payload) -> None:
            return None

    agent.nmcli = FakeNmcli()  # type: ignore[assignment]
    agent.redis = FakeRedis()  # type: ignore[assignment]

    asyncio.run(agent._handle_probe_addresses(
        CommandEnvelope(
            entry_id="1",
            request_id="probe-1",
            action="probe_addresses",
            payload={"interface": "eth0", "addresses": ["192.168.10.10", "192.168.10.11"], "timeout_sec": 1},
        )
    ))

    probe = agent._snapshot.last_address_probe
    assert probe is not None
    assert probe.request_id == "probe-1"
    assert [result.reachable for result in probe.results] == [True, False]
