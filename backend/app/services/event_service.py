from __future__ import annotations

from datetime import datetime, timezone
from typing import Any, Dict, List, Optional

from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.events.repository import EventRepository
from app.schemas.event_schema import EventCreate, EventSchema
from app.schemas.ws.events import EventLogEvent
from app.services.event_formatter import format_event_message
from app.ws.manager import WebSocketManager

_ALLOWED_FIELDS = {
    "project_id",
    "ts",
    "event_type",
    "source",
    "direction",
    "result",
    "payload",
    "message",
    "packet_id",
    "datapoint_id",
    "device_id",
    "channel_id",
}


class EventService:
    def __init__(self, db: AsyncSession):
        self.repo = EventRepository(db)

    async def list(
        self,
        project_id: Optional[int],
        *,
        limit: int = 100,
        include_system: bool = False,
        cursor: Optional[int] = None,
    ) -> List[EventSchema]:
        events = await self.repo.list(project_id, limit=limit, include_system=include_system, before_id=cursor)
        return [self._ensure_message(EventSchema.model_validate(event)) for event in events]

    async def get(self, project_id: Optional[int], event_id: int) -> Optional[EventSchema]:
        event = await self.repo.get(project_id, event_id)
        if not event:
            return None
        return self._ensure_message(EventSchema.model_validate(event))

    async def create(self, data: EventCreate | Dict[str, Any]) -> EventSchema:
        normalized = self._prepare_create_data(data)
        created = await self.repo.create(normalized)
        schema = EventSchema.model_validate(created)
        return self._ensure_message(schema)

    async def delete(self, project_id: Optional[int], event_id: int) -> bool:
        return await self.repo.delete(event_id, project_id)

    @classmethod
    async def log_and_broadcast(cls, db: AsyncSession, payload: Dict[str, Any]) -> EventSchema:
        service = cls(db)
        schema = await service.create(payload)
        event = EventLogEvent(event=schema)
        await WebSocketManager.get_instance().broadcast(event)
        return schema

    def _prepare_create_data(self, data: EventCreate | Dict[str, Any]) -> Dict[str, Any]:
        if isinstance(data, EventCreate):
            raw = data.model_dump()
        else:
            raw = dict(data)

        prepared: Dict[str, Any] = {
            key: raw[key]
            for key in _ALLOWED_FIELDS
            if key in raw and raw[key] is not None
        }

        ts_value = prepared.get("ts")
        if ts_value is not None:
            prepared["ts"] = self._coerce_timestamp(ts_value)
        elif "ts" in prepared:
            prepared.pop("ts")

        prepared["event_type"] = str(prepared.get("event_type", "system") or "system").lower()

        direction_value = prepared.get("direction")
        if direction_value is None:
            prepared.pop("direction", None)
        else:
            prepared["direction"] = self._normalize_lower(direction_value)

        result_value = prepared.get("result")
        if result_value is None:
            prepared.pop("result", None)
        else:
            prepared["result"] = self._normalize_lower(result_value)

        if "source" in prepared:
            prepared["source"] = str(prepared["source"] or "system")

        if "payload" in prepared:
            payload_value = prepared["payload"]
            if isinstance(payload_value, dict) and payload_value:
                prepared["payload"] = payload_value
            elif payload_value in (None, {}, []):
                prepared.pop("payload", None)

        if "packet_id" in prepared:
            prepared["packet_id"] = str(prepared["packet_id"])

        if not prepared.get("message"):
            prepared.pop("message", None)

        return prepared

    def _coerce_timestamp(self, value: Any) -> datetime:
        if isinstance(value, datetime):
            return value if value.tzinfo else value.replace(tzinfo=timezone.utc)
        if isinstance(value, (int, float)):
            return datetime.fromtimestamp(value / 1000, tz=timezone.utc)
        raise ValueError("Unsupported timestamp value for event")

    def _normalize_lower(self, value: Any) -> str:
        if hasattr(value, "value"):
            value = value.value
        return str(value).lower()

    def _ensure_message(self, schema: EventSchema) -> EventSchema:
        if not schema.message:
            schema.message = format_event_message(schema.model_dump())
        return schema
