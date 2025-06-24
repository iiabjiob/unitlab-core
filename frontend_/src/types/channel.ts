export interface ChannelBase {
  device_id: string
  index: number
  name?: string
  is_allocated?: boolean
  last_updated?: string
  type: 'DO' | 'DI' | 'AO' | 'AI'
}

export interface ChannelDO extends ChannelBase {
  type: 'DO'
  state: boolean
  delayBeforeMs?: number
  isPulse?: boolean
  pulseDurationMs?: number
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
}

export type Channel = ChannelDO | ChannelDI | ChannelAO
