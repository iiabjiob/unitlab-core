export type ChannelType = 'DO' | 'DI' | 'AO' | 'AI' | 'VIRTUAL'

export interface ChannelBase {
  device_id: string
  index: number
  name?: string
  is_allocated?: boolean
  lastUpdated?: string | null // ISO8601
  type: ChannelType
}

export interface ChannelDO extends ChannelBase {
  type: 'DO'
  state: boolean
  delayBeforeMs?: number
  isPulse?: boolean
  pulseMs?: number
}

export interface ChannelDI extends ChannelBase {
  type: 'DI'
  state: boolean
}

export interface ChannelAO extends ChannelBase {
  type: 'AO'
  state: number
  minValue?: number
  maxValue?: number
  unit?: 'mA' | 'V'
}

export interface ChannelAI extends ChannelBase {
  type: 'AI'
  state: number
  minValue?: number
  maxValue?: number
  unit?: 'mA' | 'V'
}

export type Channel = ChannelDO | ChannelDI | ChannelAO | ChannelAI
