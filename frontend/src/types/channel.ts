export type ChannelType = "di" | "do" | "ao"

export interface BaseChannel {
  id: number
  device_id: number
  index: number
  type: ChannelType
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

