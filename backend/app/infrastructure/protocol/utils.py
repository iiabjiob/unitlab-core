def fw_u16_to_str(u16: int) -> str:
    """Convert u16 firmware version (major<<8 | minor) to 'major.minor' string."""
    major = (u16 >> 8) & 0xFF
    minor = u16 & 0xFF
    return f"{major}.{minor}"