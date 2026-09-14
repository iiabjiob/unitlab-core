from __future__ import annotations

import pytest

from app.infrastructure.protocol.modes import Cmd
from app.services import command_queue_service


async def _capture_outbound(message):
    _capture_outbound.message = message
    return "stream-1"


@pytest.mark.anyio
async def test_do_command_assigns_distinct_command_id(monkeypatch) -> None:
    monkeypatch.setattr(command_queue_service, "enqueue_outbound_command", _capture_outbound)

    command_id = await command_queue_service.enqueue_do_command(
        unit_id="UNIT-1",
        mode=Cmd.SET_SINGLE_BIT,
        ch=2,
        value=1,
        correlation_id="test-correlation",
    )

    assert command_id
    assert _capture_outbound.message.command_id == command_id
    assert _capture_outbound.message.packet_id is not None
    assert _capture_outbound.message.correlation_id == "test-correlation"
