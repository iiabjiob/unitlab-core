"""Binary helpers for assembling and inspecting MQTT frames.

These helpers are intentionally lightweight so device simulators can safely pack
and unpack payloads without duplicating struct logic everywhere. Functions work
with bytes-like objects and perform defensive checks to avoid ValueErrors in the
core loops.
"""

from __future__ import annotations

import os
import random
import struct
from typing import Iterable, Sequence


def hex_dump(data: bytes, width: int = 16) -> str:
    """Return a hexadecimal dump similar to hexdump -C."""
    if not data:
        return "<empty>"

    lines: list[str] = []
    for offset in range(0, len(data), width):
        chunk = data[offset : offset + width]
        hex_part = " ".join(f"{b:02X}" for b in chunk)
        ascii_part = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)
        lines.append(f"{offset:08X}  {hex_part:<{width * 3}}  {ascii_part}")
    return "\n".join(lines)


def safe_pack(fmt: str, *values: object) -> bytes:
    """Pack values using struct while surfacing helpful context on failure."""
    try:
        return struct.pack(fmt, *values)
    except struct.error as exc:  # pragma: no cover - defensive
        raise ValueError(f"struct.pack failed for format={fmt!r}: {exc}") from exc


def safe_unpack(fmt: str, data: bytes) -> tuple[object, ...]:
    """Unpack data using struct with length validation."""
    size = struct.calcsize(fmt)
    if len(data) != size:
        raise ValueError(f"Expected {size} bytes for format {fmt!r}, got {len(data)}")
    try:
        return struct.unpack(fmt, data)
    except struct.error as exc:  # pragma: no cover - defensive
        raise ValueError(f"struct.unpack failed for format={fmt!r}: {exc}") from exc


def random_frame(min_len: int = 1, max_len: int = 64, *, seed: int | None = None) -> bytes:
    """Generate a random binary frame useful for fuzz or chaos testing."""
    if min_len < 0 or max_len < min_len:
        raise ValueError("Invalid length bounds for random frame generation")
    rng = random.Random(seed)
    length = rng.randint(min_len, max_len)
    return os.urandom(length)


def xor_checksum(parts: Sequence[bytes]) -> int:
    """Compute simple XOR checksum over a sequence of byte buffers."""
    checksum = 0
    for part in parts:
        for value in part:
            checksum ^= value
    return checksum


def join_payload(chunks: Iterable[bytes]) -> bytes:
    """Concatenate payload chunks with minimal overhead."""
    return b"".join(chunks)
