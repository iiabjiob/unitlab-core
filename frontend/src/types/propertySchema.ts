import type { ChannelType } from "./channel"

// базовые простые типы
export interface BasePropertyField<T> {
  key: string
  label: string | ((item: T, index?: number) => string)
  editable: boolean
  type: "string" | "number" | "boolean"
  visible?: (item: T) => boolean
  display?: (item: T) => string | number
}

// select
export interface SelectPropertyField<T> {
  key: string
  label: string | ((item: T, index?: number) => string)
  editable: true
  type: "select"
  options: string[]
  visible?: (item: T) => boolean
  display?: (item: T) => string | number
}

// unit
export interface UnitPropertyField<T> {
  key: string
  label: string | ((item: T, index?: number) => string)
  editable: true
  type: "unit"
  visible?: (item: T) => boolean
  display?: (item: T) => string | number
}

export interface BitmaskPropertyField<T> {
  key: string
  label: string
  editable: true
  type: "bitmask"
  visible?: (item: T) => boolean
  display?: (item: T) => string | number
}

// channel
export interface ChannelPropertyField<T> {
  key: string
  label: string | ((item: T, index?: number) => string)
  editable: true
  type: "channel"
  channelType: ChannelType
  visible?: (item: T) => boolean
  display?: (item: T) => string | number
}

// custom
export interface CustomPropertyField<T> {
  key: string
  label: string
  editable: boolean
  type: "custom"
  component: any
  props?: Record<string, any> | ((item: T) => Record<string, any>)
  visible?: (item: T) => boolean
  display?: (item: T) => string | number
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
