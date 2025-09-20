import type { ChannelType } from "../types/channel"

export const PROPERTY_FIELD_TYPES = {
  STRING: "string",
  NUMBER: "number",
  BOOLEAN: "boolean",
  SELECT: "select",
  UNIT: "unit",
  BITMASK: "bitmask",
  CHANNEL: "channel",
  CUSTOM: "custom",
} as const

export type PropertyFieldType = typeof PROPERTY_FIELD_TYPES[keyof typeof PROPERTY_FIELD_TYPES]

// базовые простые типы
export interface BasePropertyField<T> {
  key: string
  label: string | ((item: T, index?: number) => string)
  editable: boolean
  type: typeof PROPERTY_FIELD_TYPES.STRING | typeof PROPERTY_FIELD_TYPES.NUMBER | typeof PROPERTY_FIELD_TYPES.BOOLEAN
  visible?: (item: T) => boolean
  display?: (item: T) => string | number

  required?: boolean
  validate?: (value: any, item: T) => string | null // return error message or null
}

// select
export interface SelectPropertyField<T, O extends string = string> {
  key: string
  label: string | ((item: T, index?: number) => string)
  editable: true
  type: typeof PROPERTY_FIELD_TYPES.SELECT
  options: readonly O[]
  visible?: (item: T) => boolean
  display?: (item: T) => string | number

  required?: boolean
  validate?: (value: any, item: T) => string | null
}

// unit
export interface UnitPropertyField<T> {
  key: string
  label: string | ((item: T, index?: number) => string)
  editable: true
  type: typeof PROPERTY_FIELD_TYPES.UNIT
  visible?: (item: T) => boolean
  display?: (item: T) => string | number

  required?: boolean
  validate?: (value: any, item: T) => string | null
}

// bitmask
export interface BitmaskPropertyField<T> {
  key: string
  label: string
  editable: true
  type: typeof PROPERTY_FIELD_TYPES.BITMASK
  visible?: (item: T) => boolean
  display?: (item: T) => string | number
  resolveChannelCount: (item: T) => number

  required?: boolean
  validate?: (value: any, item: T) => string | null
}

// channel
export interface ChannelPropertyField<T> {
  key: string
  label: string | ((item: T, index?: number) => string)
  editable: true
  type: typeof PROPERTY_FIELD_TYPES.CHANNEL
  channelType: ChannelType
  visible?: (item: T) => boolean
  display?: (item: T) => string | number

  required?: boolean
  validate?: (value: any, item: T) => string | null
}

// custom
export interface CustomPropertyField<T> {
  key: string
  label: string
  editable: boolean
  type: typeof PROPERTY_FIELD_TYPES.CUSTOM
  component: any
  props?: Record<string, any> | ((item: T) => Record<string, any>)
  visible?: (item: T) => boolean
  display?: (item: T) => string | number

  required?: boolean
  validate?: (value: any, item: T) => string | null
}

// Common union
export type PropertyField<T> =
  | BasePropertyField<T>
  | SelectPropertyField<T>
  | UnitPropertyField<T>
  | BitmaskPropertyField<T>
  | ChannelPropertyField<T>
  | CustomPropertyField<T>

export interface PropertySchema<T> {
  fields: PropertyField<T>[]
  update: (item: T, key: string, value: any) => Promise<void>
}
