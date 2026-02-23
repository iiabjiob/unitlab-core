from __future__ import annotations

import json
import logging
from typing import Any

from redis.asyncio import Redis
from redis.exceptions import ResponseError

from .config import AgentConfig
from .models import CommandEnvelope

logger = logging.getLogger("unitlab.core_diag_agent.redis")


class RedisProtocol:
    def __init__(self, config: AgentConfig) -> None:
        self.config = config
        self.redis = Redis.from_url(config.redis_url, decode_responses=True)

    async def close(self) -> None:
        await self.redis.aclose()

    async def ensure_group(self) -> None:
        try:
            await self.redis.xgroup_create(
                name=self.config.redis_command_stream,
                groupname=self.config.redis_consumer_group,
                id="$",
                mkstream=True,
            )
            logger.info("Created Redis stream group %s for %s", self.config.redis_consumer_group, self.config.redis_command_stream)
        except ResponseError as exc:
            if "BUSYGROUP" in str(exc):
                logger.info("Redis stream group %s already exists", self.config.redis_consumer_group)
                return
            raise

    async def publish_event(self, event_type: str, payload: dict[str, Any]) -> None:
        body = {"event": event_type, **payload}
        await self.redis.xadd(
            self.config.redis_event_stream,
            {"json": json.dumps(body, ensure_ascii=True)},
            maxlen=self.config.redis_stream_maxlen,
            approximate=True,
        )

    async def set_state(self, snapshot: dict[str, Any]) -> None:
        await self.redis.set(self.config.redis_state_key, json.dumps(snapshot, ensure_ascii=True))

    async def read_commands(self, count: int = 10) -> list[CommandEnvelope]:
        entries = await self.redis.xreadgroup(
            groupname=self.config.redis_consumer_group,
            consumername=self.config.redis_consumer_name,
            streams={self.config.redis_command_stream: ">"},
            count=count,
            block=self.config.command_block_ms,
        )
        envelopes: list[CommandEnvelope] = []
        for _stream_name, stream_entries in entries:
            for entry_id, fields in stream_entries:
                envelope = self._parse_command(entry_id, fields)
                if envelope is None:
                    await self.publish_event("command_invalid", {"entry_id": entry_id, "fields": fields})
                    await self.ack(entry_id)
                    continue
                envelopes.append(envelope)
        return envelopes

    def _parse_command(self, entry_id: str, fields: dict[str, str]) -> CommandEnvelope | None:
        raw_json = fields.get("json")
        if not raw_json:
            return None
        try:
            payload = json.loads(raw_json)
        except json.JSONDecodeError:
            return None
        if not isinstance(payload, dict):
            return None
        action = str(payload.get("action") or "").strip()
        if not action:
            return None
        request_id = str(payload.get("request_id") or entry_id)
        return CommandEnvelope(entry_id=entry_id, request_id=request_id, action=action, payload=payload)

    async def ack(self, entry_id: str) -> None:
        await self.redis.xack(self.config.redis_command_stream, self.config.redis_consumer_group, entry_id)

