from __future__ import annotations

from app.infrastructure.protocol.modes import Cmd, State, Sys


def format_event_message(event: dict) -> str:
    etype = event.get("event_type") or event.get("type")
    payload = event.get("payload") or {}

    if etype == "status":
        parts: list[str] = []
        status = payload.get("status")
        error = payload.get("error")
        last_seen = payload.get("last_seen")
        if status is not None:
            parts.append(f"status={status}")
        if error:
            parts.append(f"error={error}")
        if last_seen is not None:
            parts.append(f"last_seen={last_seen}")
        return " ".join(parts) if parts else "status event"

    if etype == "cmd":
        unit = payload.get("unit_id")
        ch = payload.get("ch")
        val = payload.get("value")
        mode = payload.get("mode")
        pulse_ms = payload.get("pulse_ms")
        mode_name = None

        try:
            if mode in Cmd._value2member_map_:
                mode_name = Cmd(mode).name
            elif mode in State._value2member_map_:
                mode_name = State(mode).name
            elif mode in Sys._value2member_map_:
                mode_name = Sys(mode).name
        except Exception:
            pass

        parts = []
        if unit is not None:
            parts.append(f"unit={unit}")
        if ch is not None:
            parts.append(f"ch={ch}")
        if val is not None:
            parts.append(f"val={val}")
        if pulse_ms:
            parts.append(f"pulse={pulse_ms}ms")

        return "CMD " + (mode_name or str(mode)) + (" " + " ".join(parts) if parts else "")

    if etype == "sequence":
        status = payload.get("status")
        seq_id = payload.get("sequence_id")
        run_id = payload.get("run_id")
        elapsed_ms = payload.get("elapsed_ms")
        step_count = payload.get("step_count")
        error_msg = payload.get("error")
        step_index = payload.get("step_index")
        completed_steps = payload.get("completed_steps")

        if status == "started":
            detail = "started"
            if step_count is not None:
                detail += f" ({step_count} steps)"
        elif status == "completed":
            detail = "completed"
            if isinstance(completed_steps, list):
                detail += f" ({len(completed_steps)} steps)"
            if elapsed_ms is not None:
                detail += f" in {elapsed_ms} ms"
        elif status == "stopped":
            detail = "stopped"
            if isinstance(step_index, int):
                detail += f" at step {step_index + 1}"
            if elapsed_ms is not None:
                detail += f" after {elapsed_ms} ms"
        elif status == "error":
            detail = "failed"
            if isinstance(step_index, int):
                detail += f" at step {step_index + 1}"
            if elapsed_ms is not None:
                detail += f" after {elapsed_ms} ms"
            if error_msg:
                detail += f": {error_msg}"
        else:
            detail = status or "event"

        if seq_id is not None and run_id is not None:
            prefix = f"Sequence {seq_id} run {run_id}"
            return f"{prefix} {detail}".strip()

        return ", ".join(f"{k}={v}" for k, v in payload.items()) if payload else (event.get("result") or "")

    if etype == "system":
        return payload.get("msg", event.get("message", "system event"))

    return ", ".join(f"{k}={v}" for k, v in payload.items()) if payload else (event.get("result") or "")
