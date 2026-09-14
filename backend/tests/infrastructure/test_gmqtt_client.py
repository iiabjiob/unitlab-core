from __future__ import annotations

import pytest

from app.infrastructure.mqtt import gmqtt_client


@pytest.mark.anyio
async def test_mqtt_client_applies_environment_credentials_before_connect(monkeypatch) -> None:
    calls: list[tuple[str, tuple]] = []

    class FakeClient:
        def __init__(self, client_id: str) -> None:
            self.client_id = client_id

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
