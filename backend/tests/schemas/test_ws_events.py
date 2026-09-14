from app.infrastructure.protocol.packet_structures import RespError, RespStatus
from app.schemas.ws.events import DeviceRespEvent, HardwareCommandResultEvent, WSChannel


def test_device_resp_event_serializes_status_and_error_names() -> None:
    event = DeviceRespEvent(
        unit_id="do-001",
        packet_id=42,
        status=RespStatus.OK,
        error=RespError.NONE,
        timestamp=123456789,
    )

    payload = event.model_dump(mode="json")

    assert payload["channel"] == WSChannel.DEVICE_RESP
    assert payload["status"] == "OK"
    assert payload["error"] == "NONE"


def test_hardware_command_result_event_has_stable_delivery_contract() -> None:
    payload = HardwareCommandResultEvent(
        command_id="cmd-1",
        delivery="rejected",
        reason="channel_lease_busy",
    ).model_dump(mode="json")

    assert payload == {
        "channel": WSChannel.HARDWARE_COMMAND_RESULT,
        "event": "hardware_command_result",
        "command_id": "cmd-1",
        "delivery": "rejected",
        "execution": "unknown",
        "reason": "channel_lease_busy",
    }
