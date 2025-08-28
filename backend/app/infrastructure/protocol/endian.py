# protocol/endian.py
# Big-endian helpers for fixed-width unsigned integers.
# Functions support two styles:
#   - Bytes-returning: write_u16_be(value) -> bytes
#   - In-place write:  write_u16_be(value, into=bytearray, offset=pos) -> None

from typing import Optional, Union

MutableBuf = Union[bytearray, memoryview]

def _check_uint(value: int, bits: int) -> None:
    if not isinstance(value, int):
        raise TypeError(f"value must be int, got {type(value).__name__}")
    if value < 0 or value > ((1 << bits) - 1):
        raise ValueError(f"value out of range for u{bits}: {value}")

def _ensure_room(buf: MutableBuf, offset: int, size: int) -> None:
    if offset < 0:
        raise ValueError("offset must be >= 0")
    if offset + size > len(buf):
        raise ValueError(f"buffer too small: need {offset + size}, have {len(buf)}")

# -------------------------
# Write (big-endian)
# -------------------------

def write_u16_be(value: int, into: Optional[MutableBuf] = None, offset: int = 0):
    """Write u16 in big-endian order. Returns bytes unless 'into' buffer is given."""
    _check_uint(value, 16)
    b = value.to_bytes(2, byteorder="big")
    if into is None:
        return b
    _ensure_room(into, offset, 2)
    into[offset:offset+2] = b

def write_u32_be(value: int, into: Optional[MutableBuf] = None, offset: int = 0):
    """Write u32 in big-endian order. Returns bytes unless 'into' buffer is given."""
    _check_uint(value, 32)
    b = value.to_bytes(4, byteorder="big")
    if into is None:
        return b
    _ensure_room(into, offset, 4)
    into[offset:offset+4] = b

def write_u64_be(value: int, into: Optional[MutableBuf] = None, offset: int = 0):
    """Write u64 in big-endian order. Returns bytes unless 'into' buffer is given."""
    _check_uint(value, 64)
    b = value.to_bytes(8, byteorder="big")
    if into is None:
        return b
    _ensure_room(into, offset, 8)
    into[offset:offset+8] = b

# -------------------------
# Read (big-endian)
# -------------------------

def read_u16_be(buf: bytes, offset: int = 0) -> int:
    """Read u16 (big-endian) from 'buf' starting at 'offset'."""
    if offset < 0:
        raise ValueError("offset must be >= 0")
    end = offset + 2
    if end > len(buf):
        raise ValueError("buffer too small for u16")
    return int.from_bytes(buf[offset:end], byteorder="big", signed=False)

def read_u32_be(buf: bytes, offset: int = 0) -> int:
    """Read u32 (big-endian) from 'buf' starting at 'offset'."""
    if offset < 0:
        raise ValueError("offset must be >= 0")
    end = offset + 4
    if end > len(buf):
        raise ValueError("buffer too small for u32")
    return int.from_bytes(buf[offset:end], byteorder="big", signed=False)

def read_u64_be(buf: bytes, offset: int = 0) -> int:
    """Read u64 (big-endian) from 'buf' starting at 'offset'."""
    if offset < 0:
        raise ValueError("offset must be >= 0")
    end = offset + 8
    if end > len(buf):
        raise ValueError("buffer too small for u64")
    return int.from_bytes(buf[offset:end], byteorder="big", signed=False)