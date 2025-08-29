// src/types/channel.ts

export type ChannelType = "DI" | "DO" | "AO"

export interface BaseChannel {
  /** Номер канала в устройстве (с 0) */
  index: number

  /** К какому устройству принадлежит */
  device_id: string

  /** Тип канала */
  type: ChannelType

  /** Имя канала (пользовательское или auto-generated) */
  name?: string
}

/** Цифровой вход (DI) */
export interface DiChannel extends BaseChannel {
  type: "DI"
  /** Состояние входа: true=замкнут, false=разомкнут */
  state: boolean
}

/** Цифровой выход (DO) */
export interface DoChannel extends BaseChannel {
  type: "DO"
  /** Состояние выхода: true=ON, false=OFF */
  state: boolean
}

/** Аналоговый выход (AO) */
export interface AoChannel extends BaseChannel {
  type: "AO"
  /** Текущее значение, мА (4..20) */
  state: number
}

/** Универсальный канал */
export type Channel = DiChannel | DoChannel | AoChannel
