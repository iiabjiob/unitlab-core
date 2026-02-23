import type { Channel, ChannelDto } from "./channel"

export type DeviceType = "do" | "di" | "ao"

export type DeviceStatus = "online" | "offline"

export type DeviceHeartbeatFastSnapshot = Record<string, unknown>
export type DeviceHeartbeatDiagSnapshot = Record<string, unknown>

export interface DeviceDto {
  id: number
  unit_id: string
  device_type: DeviceType
  num_channels?: number | null
  firmware_version?: string | null
  name?: string | null
  status?: DeviceStatus | string
  last_seen?: number | null
  registered_at?: number | null
  channels?: Array<ChannelDto | Channel> | null
  heartbeat_fast?: DeviceHeartbeatFastSnapshot | null
  heartbeat_diag?: DeviceHeartbeatDiagSnapshot | null
}

export interface Device {
  id: number
  unit_id: string
  display_name: string
  device_type: DeviceType
  num_channels: number
  channels: Channel[]
  online: boolean
  firmware_version?: string
  last_seen?: number
  registered_at?: number

  // compatibility fields for legacy UI (remove later)
  type: DeviceType
  status: DeviceStatus
  is_active?: boolean
  name?: string | null
  location?: string | null
  heartbeat_fast?: DeviceHeartbeatFastSnapshot | null
  heartbeat_diag?: DeviceHeartbeatDiagSnapshot | null
}

export interface DeviceBulkDeletePayload {
  ids: number[]
}

export interface DeviceBulkDeleteResponse {
  deleted: number
}
