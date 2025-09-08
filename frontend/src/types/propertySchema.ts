// types/propertySchema.ts
export interface PropertyField<T> {
  key: Extract<keyof T, string>
  label: string
  editable: boolean
  type: "string" | "number" | "boolean" | "enum"
}

export interface PropertySchema<T> {
  fields: PropertyField<T>[]
  update: (item: T, key: keyof T, value: any) => Promise<void>
}
