export interface Device {
  unit_id: string
  type: string   // DI | DO | AO
  channels: number
  firmware_version: number
  is_active: boolean
  location?: string
  status: "online" | "offline"
  last_seen?: number
}
