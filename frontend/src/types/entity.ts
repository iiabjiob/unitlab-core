// src/types/entity.ts
import type { Channel } from "@/types/channel"
import type { Device } from "@/types/device"
import type { Switchgear } from "@/types/switchgear"
import type { SequenceDef, SequenceStep } from "./sequences"
import type { SCHEMA_NAMES } from "@/property-schemas/types"

export type EntityType = typeof SCHEMA_NAMES[keyof typeof SCHEMA_NAMES]

export type EntityMap = {
  channel: Channel
  device: Device
  switchgear: Switchgear
  sequence: SequenceDef
  sequence_step: SequenceStep
}
