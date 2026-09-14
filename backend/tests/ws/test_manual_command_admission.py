from __future__ import annotations

import asyncio
from types import SimpleNamespace

from app.schemas.ws.messages import SetAoCommandMessage, WSAction
from app.ws.actions import manual_command_admission


def run_async(awaitable):
    return asyncio.run(awaitable)


def test_manual_ao_passes_single_channel_as_channel_ids(monkeypatch) -> None:
    captured: dict = {}

    async def channel(*args, **kwargs):
        return SimpleNamespace(id=17, device_id=23)

    async def enqueue_manual(ws, **kwargs):
        captured.update(kwargs)

    monkeypatch.setattr(manual_command_admission, "_channel", channel)
    monkeypatch.setattr(manual_command_admission, "_enqueue_manual", enqueue_manual)

    message = SetAoCommandMessage(
        action=WSAction.SET_AO_COMMAND,
        workspace_id=7,
        channel_id=17,
        unit_id="unit-1",
        ch=2,
        value=12.5,
    )
    run_async(manual_command_admission.handle_manual_ao(object(), message))

    assert captured["channel_ids"] == [17]
    assert captured["device_id"] == 23
    assert captured["action"] == "ao_set"
