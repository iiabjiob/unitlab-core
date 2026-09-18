from app.infrastructure.mqtt.handlers import bootstrap  # noqa: F401
from app.infrastructure.mqtt.router import MqttRouter


def test_device_runtime_topics_have_registered_handlers() -> None:
    router = MqttRouter()

    expected_topics = {
        "DO-001/h": "handle_device_heartbeat",
        "DO-001/hd": "handle_device_heartbeat",
        "DO-001/reg": "handle_device_register",
        "DO-001/r": "handle_device_resp",
        "DO-001/s": "handle_device_state",
    }

    for topic, handler_name in expected_topics.items():
        handler, pattern = router.find_handler(topic)
        assert handler is not None
        assert pattern is not None
        assert handler.__name__ == handler_name
