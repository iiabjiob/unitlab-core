from __future__ import annotations

import time
import uuid
from dataclasses import dataclass, field
from enum import Enum
from typing import cast


class SequenceCommandType(str, Enum):
    START = "start"
    STOP = "stop"


@dataclass(slots=True)
class SequenceCommand:
    type: SequenceCommandType
    sequence_id: int
    run_id: int | None = None
    requested_by: str | None = None
    request_id: str = field(default_factory=lambda: uuid.uuid4().hex)
    enqueued_at_ms: int = field(default_factory=lambda: int(time.time() * 1000))
    extra: dict[str, object] = field(default_factory=dict)
    attempts: int = 0

    def to_payload(self) -> dict[str, object]:
        return {
            "type": self.type.value,
            "sequence_id": self.sequence_id,
            "run_id": self.run_id,
            "requested_by": self.requested_by,
            "request_id": self.request_id,
            "enqueued_at_ms": self.enqueued_at_ms,
            "extra": self.extra,
            "attempts": self.attempts,
        }

    @staticmethod
    def from_payload(payload: dict[str, object]) -> "SequenceCommand":
        sequence_id = payload.get("sequence_id")
        if sequence_id is None:
            raise ValueError("sequence_id is required in sequence command payload")
        return SequenceCommand(
            type=SequenceCommandType(cast(str, payload["type"])),
            sequence_id=int(cast(int | str | float, sequence_id)),
            run_id=cast(int | None, payload.get("run_id")),
            requested_by=cast(str | None, payload.get("requested_by")),
            request_id=cast(str | None, payload.get("request_id")) or uuid.uuid4().hex,
            enqueued_at_ms=int(cast(int | str | float, payload.get("enqueued_at_ms", int(time.time() * 1000)))),
            extra=cast(dict[str, object], payload.get("extra") or {}),
            attempts=int(cast(int | str | float, payload.get("attempts", 0) or 0)),
        )

    def bumped_attempt(self, new_attempts: int, *, extra: dict[str, object] | None = None) -> "SequenceCommand":
        payload_extra = dict(self.extra)
        if extra:
            payload_extra.update(extra)
        return SequenceCommand(
            type=self.type,
            sequence_id=self.sequence_id,
            run_id=self.run_id,
            requested_by=self.requested_by,
            request_id=self.request_id,
            enqueued_at_ms=self.enqueued_at_ms,
            extra=payload_extra,
            attempts=new_attempts,
        )


class SequenceEventType(str, Enum):
    STARTED = "started"
    STOPPING = "stopping"
    STEP_STARTED = "step_started"
    STEP_COMPLETED = "step_completed"
    STEP_ISSUE = "step_issue"
    FINISHED = "finished"
    FAILED = "failed"


@dataclass(slots=True)
class SequenceEvent:
    type: SequenceEventType
    sequence_id: int
    run_id: int
    data: dict[str, object] = field(default_factory=dict)
    emitted_at_ms: int = field(default_factory=lambda: int(time.time() * 1000))

    def to_payload(self) -> dict[str, object]:
        return {
            "type": self.type.value,
            "sequence_id": self.sequence_id,
            "run_id": self.run_id,
            "data": self.data,
            "emitted_at_ms": self.emitted_at_ms,
        }

    @staticmethod
    def from_payload(payload: dict[str, object]) -> "SequenceEvent":
        return SequenceEvent(
            type=SequenceEventType(cast(str, payload["type"])),
            sequence_id=int(cast(int | str | float, payload["sequence_id"])),
            run_id=int(cast(int | str | float, payload["run_id"])),
            data=cast(dict[str, object], payload.get("data") or {}),
            emitted_at_ms=int(cast(int | str | float, payload.get("emitted_at_ms", int(time.time() * 1000)))),
        )
