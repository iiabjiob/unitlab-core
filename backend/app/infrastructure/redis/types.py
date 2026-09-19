from __future__ import annotations

from typing import Protocol


RedisStreamEntry = tuple[str, dict[str, object]]
RedisStreamEntries = list[RedisStreamEntry]


class RedisStreamClient(Protocol):
    async def xgroup_create(self, *args: object, **kwargs: object) -> object: ...

    async def xreadgroup(
        self,
        *args: object,
        **kwargs: object,
    ) -> list[tuple[str, RedisStreamEntries]]: ...

    async def xack(self, *args: object) -> object: ...


class RedisHashClient(Protocol):
    async def get(self, name: str) -> str | None: ...

    async def set(self, name: str, value: object, **kwargs: object) -> None: ...

    async def hget(self, name: str, key: str) -> str | None: ...

    async def hset(self, *args: object, **kwargs: object) -> None: ...

    async def hdel(self, *args: object) -> None: ...

    async def hgetall(self, name: str) -> dict[str, str]: ...

    async def hlen(self, name: str) -> int: ...

    async def hkeys(self, name: str) -> list[str]: ...
