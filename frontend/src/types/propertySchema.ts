// базовые простые типы
export interface BasePropertyField<T> {
  key: Extract<keyof T, string>
  label: string
  editable: boolean
  type: "string" | "number" | "boolean" | "enum"
}

// новый интерфейс под каналы
export interface ChannelPropertyField<T> {
  key: Extract<keyof T, string>
  label: string
  editable: true
  type: "channel"
  channelType: "do" | "di"
}

// кастомный (как у тебя было)
export interface CustomPropertyField {
  key: string
  label: string
  editable: false
  type: "custom"
  component: any
  props?: Record<string, any> | ((item: any) => Record<string, any>)
}

// общий union
export type PropertyField<T> =
  | BasePropertyField<T>
  | ChannelPropertyField<T>
  | CustomPropertyField

export interface PropertySchema<T> {
  fields: PropertyField<T>[]
  update: (item: T, key: keyof T, value: any) => Promise<void>
}
