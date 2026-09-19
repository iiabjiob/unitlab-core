from __future__ import annotations

import json
from types import SimpleNamespace

import pytest

from app.services import core_network_service
from app.services.core_network_service import enqueue_core_network_command


class _FakeRedis:
    def __init__(self) -> None:
        self.calls: list[dict[str, object]] = []

    async def xadd(self, stream: str, fields: dict[str, str], *, maxlen: int, approximate: bool) -> None:
        self.calls.append(
            {
                "stream": stream,
                "fields": fields,
                "maxlen": maxlen,
                "approximate": approximate,
            }
        )


@pytest.mark.anyio
async def test_enqueue_core_network_command_serializes_apply_settings_payload(monkeypatch: pytest.MonkeyPatch) -> None:
    fake = _FakeRedis()
    monkeypatch.setattr(core_network_service.RedisManager, "get_instance", staticmethod(lambda: fake))  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(core_network_service, "settings", SimpleNamespace(core_net_command_stream="core_net:commands", core_net_command_stream_maxlen=1234))

    accepted = await enqueue_core_network_command(
        "apply_network_settings",
        request_id="req-123",
        payload={
            "interface": "eth0",
            "profile": "unitlab-lan",
            "ipv4_mode": "manual",
            "address_cidr": "192.168.10.21/24",
            "gateway": "192.168.10.1",
            "dns_servers": ["192.168.10.1"],
            "proxy_url": "http://proxy:3128",
            "proxy_no_proxy": ["localhost"],
        },
    )

    assert accepted.request_id == "req-123"
    assert accepted.action == "apply_network_settings"
    assert fake.calls == [
        {
            "stream": "core_net:commands",
            "fields": {"json": json.dumps(
                {
                    "request_id": "req-123",
                    "action": "apply_network_settings",
                    "interface": "eth0",
                    "profile": "unitlab-lan",
                    "ipv4_mode": "manual",
                    "address_cidr": "192.168.10.21/24",
                    "gateway": "192.168.10.1",
                    "dns_servers": ["192.168.10.1"],
                    "proxy_url": "http://proxy:3128",
                    "proxy_no_proxy": ["localhost"],
                },
                ensure_ascii=True,
            )},
            "maxlen": 1234,
            "approximate": True,
        }
    ]


@pytest.mark.anyio
async def test_enqueue_core_network_command_serializes_probe_payload(monkeypatch: pytest.MonkeyPatch) -> None:
    fake = _FakeRedis()
    monkeypatch.setattr(core_network_service.RedisManager, "get_instance", staticmethod(lambda: fake))  # pyright: ignore[reportPrivateLocalImportUsage]
    monkeypatch.setattr(core_network_service, "settings", SimpleNamespace(core_net_command_stream="core_net:commands", core_net_command_stream_maxlen=1234))

    accepted = await enqueue_core_network_command(
        "probe_addresses",
        request_id="req-probe",
        payload={
            "interface": "eth0",
            "addresses": ["192.168.10.10", "192.168.10.21"],
            "timeout_sec": 1,
        },
    )

    assert accepted.request_id == "req-probe"
    assert accepted.action == "probe_addresses"
    assert fake.calls == [
        {
            "stream": "core_net:commands",
            "fields": {"json": json.dumps(
                {
                    "request_id": "req-probe",
                    "action": "probe_addresses",
                    "interface": "eth0",
                    "addresses": ["192.168.10.10", "192.168.10.21"],
                    "timeout_sec": 1,
                },
                ensure_ascii=True,
            )},
            "maxlen": 1234,
            "approximate": True,
        }
    ]
