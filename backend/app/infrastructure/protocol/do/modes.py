
# Digital Outputs (range 0x20..0x2F)
REQ_STATE_SINGLE = 0x20  # [ch]
REQ_STATE_ALL    = 0x21  # []
STATE_SINGLE     = 0x22  # [ch][val:u8]
STATE_ALL        = 0x23  # [bitmap:u32 BE]
CMD_SET_SINGLE   = 0x28  # [ch][val:u8]
CMD_SET_ALL      = 0x29  # [bitmap:u32 BE]
CMD_SET_PAIR     = 0x2C  # [chA][chB][state2b]
