// utils/channel.ts

import { CHANNEL_TYPES, type Channel, type ChannelDto } from "@/types/channel"

export function normalizeChannel(dto: ChannelDto): Channel {
  const base = {
    id: dto.id,
    device_id: dto.device_id,
    index: dto.channel_index,
    name: dto.name?.trim() ?? "",
    resolved_name: dto.resolved_name?.trim() ?? "",
    created_at: dto.created_at ? Date.parse(dto.created_at) : undefined,
    updated_at: dto.updated_at ? Date.parse(dto.updated_at) : undefined,
  }

  if (dto.channel_type === CHANNEL_TYPES.DI) {
    return {
      ...base,
      type: CHANNEL_TYPES.DI,
      state: Boolean(dto.state),
    }
  }

  if (dto.channel_type === CHANNEL_TYPES.DO) {
    return {
      ...base,
      type: CHANNEL_TYPES.DO,
      state: Boolean(dto.state),
    }
  }

  return {
    ...base,
    type: CHANNEL_TYPES.AO,
    state: typeof dto.state === "number" ? dto.state : 0,
  }
}

export function ensureChannel(entity: Channel | ChannelDto): Channel {
  if ("channel_index" in entity || "channel_type" in entity) {
    return normalizeChannel(entity as ChannelDto)
  }
  return entity as Channel
}

/** Ограничивает значение в диапазоне 4..20 мА (или 0..24, если хочешь расширенный режим) */
export function clampAoValue(value: number, min = 4, max = 20): number {
  if (isNaN(value)) return min
  if (value < min) return min
  if (value > max) return max
  return value
}

/** Приводит значение к строке с двумя знаками после запятой */
export function formatAoValue(value: number): string {
  return clampAoValue(value).toFixed(2)
}

/** Парсинг строки из input → нормализованное число */
export function parseAoInput(raw: string, min = 4, max = 20): number {
  const num = parseFloat(raw)
  return clampAoValue(isNaN(num) ? min : num, min, max)
}
