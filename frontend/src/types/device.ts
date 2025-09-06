import type { ChannelType } from "./channel"

export interface Device {
  unit_id: string
  type: ChannelType
  channels: number
  firmware_version: number
  is_active: boolean
  location?: string
  status: "online" | "offline"
  last_seen?: number
}
