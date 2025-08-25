def write_u16_be(value: int) -> bytes:
    return value.to_bytes(2, "big", signed=False)

def write_u32_be(value: int) -> bytes:
    return value.to_bytes(4, "big", signed=False)

def write_u64_be(value: int) -> bytes:
    return value.to_bytes(8, "big", signed=False)

def write_f32_be(value: float) -> bytes:
    import struct
    return struct.pack(">f", float(value))

def read_u16_be(b: bytes) -> int:
    return int.from_bytes(b, "big", signed=False)

def read_u32_be(b: bytes) -> int:
    return int.from_bytes(b, "big", signed=False)

def read_u64_be(b: bytes) -> int:
    return int.from_bytes(b, "big", signed=False)

def read_f32_be(b: bytes) -> float:
    import struct
    return struct.unpack(">f", b)[0]
