
# Analog Outputs (range 0x30..0x3F)
REQ_STATE_SINGLE = 0x30  # [ch]
REQ_STATE_ALL    = 0x31  # []
STATE_SINGLE     = 0x32  # [ch][value:floatBE]
STATE_ALL        = 0x33  # [floatBE]*N
CMD_SET_SINGLE   = 0x38  # [ch][value:floatBE]
CMD_SET_ALL      = 0x39  # [count_ch:1][floatBE]*N
