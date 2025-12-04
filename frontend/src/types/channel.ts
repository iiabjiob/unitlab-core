export const CHANNEL_TYPES = {
  DI: "di" as const,
  DO: "do" as const,
  AO: "ao" as const,
}

export type ChannelType = typeof CHANNEL_TYPES[keyof typeof CHANNEL_TYPES]

export interface ChannelDto {
  id: number
  device_id: number
  channel_index: number
  channel_type: ChannelType
  name?: string | null
  resolved_name?: string | null
  state?: boolean | number | null
  created_at?: string
  updated_at?: string
}

export interface ChannelBase {
  id: number
  device_id: number
  index: number
  type: ChannelType
  name: string
  resolved_name: string
  created_at?: number
  updated_at?: number
}

export interface DiChannel extends ChannelBase {
  type: "di"
  state: boolean
}

export interface DoChannel extends ChannelBase {
  type: "do"
  state: boolean
}

export interface AoChannel extends ChannelBase {
  type: "ao"
  state: number
}

export type Channel = DiChannel | DoChannel | AoChannel

