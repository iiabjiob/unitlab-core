from pathlib import Path


CONFIG_DIR = Path(__file__).resolve().parents[3] / "config"


def _config(name: str) -> str:
    return (CONFIG_DIR / name).read_text(encoding="utf-8")


def test_secure_mqtt_profile_disables_anonymous_access_and_scopes_auth() -> None:
    config = _config("mosquitto.secure.conf")
    acl = _config("mosquitto.secure.acl")

    assert "allow_anonymous false" in config
    assert "password_file /mosquitto/config/passwordfile" in config
    assert "acl_file /mosquitto/config/aclfile" in config
    assert "user unitlab-backend" in acl
    assert "topic write +/c" in acl
    assert "pattern read %u/c" in acl
    assert "pattern write %u/s" in acl


def test_mqtt_tls_overlay_exposes_only_authenticated_tls_listener() -> None:
    config = _config("mosquitto.secure.tls.conf")

    assert "listener 8883 0.0.0.0" in config
    assert "listener 1883" not in config
    assert "allow_anonymous false" in config
    assert "cafile /mosquitto/config/certs/ca.crt" in config
    assert "certfile /mosquitto/config/certs/server.crt" in config
    assert "keyfile /mosquitto/config/certs/server.key" in config
