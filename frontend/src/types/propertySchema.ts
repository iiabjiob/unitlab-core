// types/propertySchema.ts
export interface BasePropertyField<T> {
  key: Extract<keyof T, string>
  label: string
  editable: boolean
  type: "string" | "number" | "boolean" | "enum" | "signal"
}

export interface CustomPropertyField {
  key: string
  label: string
  editable: false   // кастомные поля сами управляют редактированием
  type: "custom"
  component: any
  props?: Record<string, any> | ((item: any) => Record<string, any>)
}

export type PropertyField<T> = BasePropertyField<T> | CustomPropertyField

export interface PropertySchema<T> {
  fields: PropertyField<T>[]
  update: (item: T, key: keyof T, value: any) => Promise<void>
}

