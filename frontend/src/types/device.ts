import type { Channel, ChannelType } from "./channel"

export interface Device {
  id: number
  unit_id: string
  type: ChannelType
  num_channels: number
  firmware_version?: string
  is_active: boolean
  name?: string
  location?: string
  status: "online" | "offline"
  last_seen?: number

  channels?: Channel[]
}
