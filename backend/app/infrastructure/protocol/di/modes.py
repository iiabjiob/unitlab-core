
# Digital Inputs (range 0x10..0x1F)
REQ_STATE_SINGLE = 0x10  # [ch]
REQ_STATE_ALL    = 0x11  # []
STATE_SINGLE     = 0x12  # [ch][val:u8]
STATE_ALL        = 0x13  # [bitmap:u32 BE]
