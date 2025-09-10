// types/propertySchema.ts
export interface PropertyField<T> {
  key: Extract<keyof T, string>
  label: string
  editable: boolean
  type: "string" | "number" | "boolean" | "enum" | "signal"
  signalKind?: "DI" | "DO"
  unitKey?: Extract<keyof T, string>   // куда писать unitId
  channelKey?: Extract<keyof T, string> // куда писать номер канала
}

export interface PropertySchema<T> {
  fields: PropertyField<T>[]
  update: (item: T, key: keyof T, value: any) => Promise<void>
}
