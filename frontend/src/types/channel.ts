export type ChannelType = "DI" | "DO" | "AO"

export interface BaseChannel {
  index: number
  type: ChannelType
  device_id: string
  name?: string
}

/** Цифровой вход (DI) */
export interface DiChannel extends BaseChannel {
  /** Состояние входа: true=замкнут, false=разомкнут */
  state: boolean
}

/** Цифровой выход (DO) */
export interface DoChannel extends BaseChannel {
  /** Состояние выхода: true=ON, false=OFF */
  state: boolean
}

/** Аналоговый выход (AO) */
export interface AoChannel extends BaseChannel {
  /** Текущее значение, мА (4..20) */
  state: number
}

/** Универсальный канал */
export type Channel = DiChannel | DoChannel | AoChannel

