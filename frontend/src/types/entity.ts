// src/types/entity.ts
import type { Channel } from "@/types/channel"
import type { Device } from "@/types/device"
import type { Switchgear } from "@/types/switchgear"

export type EntityType = "channel" | "device" | "switchgear"

export type EntityMap = {
  channel: Channel
  device: Device
  switchgear: Switchgear
}
