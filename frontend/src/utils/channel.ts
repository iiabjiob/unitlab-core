// utils/channel.ts

import { CHANNEL_TYPES, type Channel, type ChannelDto, type ChannelType } from "@/types/channel"

function normalizeChannelType(raw: unknown): ChannelType | null {
  const value = String(raw ?? "").trim().toLowerCase()
  if (value === CHANNEL_TYPES.DI) return CHANNEL_TYPES.DI
  if (value === CHANNEL_TYPES.DO) return CHANNEL_TYPES.DO
  if (value === CHANNEL_TYPES.AO) return CHANNEL_TYPES.AO
  return null
}

export function normalizeChannel(dto: ChannelDto, fallbackType?: ChannelType | null): Channel {
  const raw = dto as unknown as Record<string, unknown>
  const channelType = normalizeChannelType(dto.channel_type ?? raw.type) ?? normalizeChannelType(fallbackType)
  const channelIndexRaw = dto.channel_index ?? raw.index
  const channelIndex = Number.isFinite(Number(channelIndexRaw)) ? Number(channelIndexRaw) : 0
  const stateRaw = dto.state ?? raw.state
  const createdAtRaw = dto.created_at ?? raw.created_at
  const updatedAtRaw = dto.updated_at ?? raw.updated_at
  const createdAt = typeof createdAtRaw === "number"
    ? createdAtRaw
    : (createdAtRaw ? Date.parse(String(createdAtRaw)) : undefined)
  const updatedAt = typeof updatedAtRaw === "number"
    ? updatedAtRaw
    : (updatedAtRaw ? Date.parse(String(updatedAtRaw)) : undefined)
  const base = {
    id: dto.id,
    device_id: dto.device_id,
    index: channelIndex,
    name: dto.name?.trim() ?? "",
    resolved_name: dto.resolved_name?.trim() ?? String(raw.resolved_name ?? "").trim(),
    created_at: Number.isFinite(createdAt as number) ? createdAt : undefined,
    updated_at: Number.isFinite(updatedAt as number) ? updatedAt : undefined,
  }

  if (channelType === CHANNEL_TYPES.DI) {
    return {
      ...base,
      type: CHANNEL_TYPES.DI,
      state: Boolean(stateRaw),
    }
  }

  if (channelType === CHANNEL_TYPES.DO) {
    return {
      ...base,
      type: CHANNEL_TYPES.DO,
      state: Boolean(stateRaw),
      ui: { stage: "idle" },
    }
  }

  if (channelType === CHANNEL_TYPES.AO) {
    return {
      ...base,
      type: CHANNEL_TYPES.AO,
      state: typeof stateRaw === "number" ? stateRaw : 0,
    }
  }

  // Unknown type: keep controls hidden (non-AO fallback) until explicit type arrives.
  return {
    ...base,
    type: CHANNEL_TYPES.DI,
    state: Boolean(stateRaw),
  }
}

export function ensureChannel(entity: Channel | ChannelDto, fallbackType?: ChannelType | null): Channel {
  return normalizeChannel(entity as ChannelDto, fallbackType)
}

export function buildBitmask(channels: Channel[], state: boolean): number {
  const mask = channels.reduce((mask, ch) => {
    if (ch.type !== CHANNEL_TYPES.DO) return mask
    const bit = 1 << ch.index
    return state ? (mask | bit) : (mask & ~bit)
  }, 0)

  return mask >>> 0
}

export function buildToggleBitmask(channels: Channel[]): number {
  const mask = channels.reduce((mask, ch) => {
    if (ch.type !== CHANNEL_TYPES.DO) return mask
    const bit = 1 << ch.index
    return ch.state ? (mask & ~bit) : (mask | bit)
  }, 0)

  return mask >>> 0
}

/** Clamp AO value to the safe 4–20 mA range (override via args for custom windows). */
export function clampAoValue(value: number, min = 4, max = 20): number {
  if (isNaN(value)) return min
  if (value < min) return min
  if (value > max) return max
  return value
}

/** Format AO value with two decimals so inputs stay consistent with backend expectations. */
export function formatAoValue(value: number): string {
  return clampAoValue(value).toFixed(2)
}

/** Parse user input into a normalized AO number while enforcing bounds. */
export function parseAoInput(raw: string, min = 4, max = 20): number {
  const num = parseFloat(raw)
  return clampAoValue(isNaN(num) ? min : num, min, max)
}
