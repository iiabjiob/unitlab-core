from __future__ import annotations

import pytest
from pathlib import Path

from app.infrastructure.mqtt import gmqtt_client


@pytest.mark.anyio
async def test_mqtt_client_applies_environment_credentials_before_connect(monkeypatch: pytest.MonkeyPatch) -> None:
    calls: list[tuple[str, tuple[object, ...]]] = []

    class FakeClient:
        def __init__(self, client_id: str) -> None:
            self.client_id: str = client_id

        def set_auth_credentials(self, username: str, password: str) -> None:
            calls.append(("auth", (username, password)))

        async def connect(self, host: str, port: int) -> None:
            calls.append(("connect", (host, port)))

    monkeypatch.setattr(gmqtt_client, "MQTTClient", FakeClient)
    client = gmqtt_client.UnitLabMqttClient("unitlab-test")

    await client.connect("mqtt.local", 1883, username="backend", password="secret")

    assert calls == [
        ("auth", ("backend", "secret")),
        ("connect", ("mqtt.local", 1883)),
    ]


@pytest.mark.anyio
async def test_mqtt_client_rejects_tls_without_ca_file(monkeypatch: pytest.MonkeyPatch) -> None:
    class FakeClient:
        def __init__(self, client_id: str) -> None:
            del client_id

    monkeypatch.setattr(gmqtt_client, "MQTTClient", FakeClient)
    client = gmqtt_client.UnitLabMqttClient("unitlab-test")

    with pytest.raises(ValueError, match="requires a CA file"):
        await client.connect("mqtt.local", 8883, tls=True)


@pytest.mark.anyio
async def test_mqtt_client_passes_tls_context_to_gmqtt(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    calls: list[tuple[str, tuple[object, ...], dict[str, object]]] = []
    ca_file = tmp_path / "ca.pem"
    _ = ca_file.write_text("not-a-real-certificate")

    class FakeClient:
        def __init__(self, client_id: str) -> None:
            del client_id

        async def connect(self, host: str, port: int, **kwargs: object) -> None:
            calls.append(("connect", (host, port), kwargs))

    monkeypatch.setattr(gmqtt_client, "MQTTClient", FakeClient)
    def _tls_context(**kwargs: object) -> str:
        del kwargs
        return "tls-context"

    monkeypatch.setattr(gmqtt_client.ssl, "create_default_context", _tls_context)  # pyright: ignore[reportPrivateLocalImportUsage]
    client = gmqtt_client.UnitLabMqttClient("unitlab-test")

    await client.connect("mqtt.local", 8883, tls=True, tls_ca_file=str(ca_file))

    assert calls == [("connect", ("mqtt.local", 8883), {"ssl": "tls-context"})]
