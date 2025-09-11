def fw_u16_to_str(u16: int) -> str:
    """Convert u16 firmware version (major<<8 | minor) to 'major.minor' string."""
    major = (u16 >> 8) & 0xFF
    minor = u16 & 0xFF
    return f"{major}.{minor}"

def fw_str_to_u16(version: str) -> int:
    """Convert 'major.minor' string back to u16 (major<<8 | minor)."""
    try:
        major_str, minor_str = version.split(".")
        major = int(major_str)
        minor = int(minor_str)
    except (ValueError, AttributeError) as e:
        raise ValueError(f"Invalid firmware version string: '{version}'") from e

    if not (0 <= major <= 255 and 0 <= minor <= 255):
        raise ValueError(f"Firmware version out of range: {major}.{minor}")

    return (major << 8) | minor