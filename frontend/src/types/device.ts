import type { ChannelType } from "./channel"

export interface Device {
  unit_id: string
  type: ChannelType
  num_channels: number
  firmware_version: number
  is_active: boolean
  name?: string
  location?: string
  status: "online" | "offline"
  last_seen?: number
}
