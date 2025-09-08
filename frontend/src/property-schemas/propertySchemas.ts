// config/propertySchemas.ts
import { devicePropertySchema } from "./devicePropertySchema"
import type { PropertySchema } from "@/types/propertySchema"
import type { Device } from "@/types/device"

export type EntityType = "device"
export type EntityMap = {
  device: Device
}

export const propertySchemas: {
  [K in EntityType]: PropertySchema<EntityMap[K]>
} = {
  device: devicePropertySchema,
}

// fallback для неизвестного типа
export const emptySchema: PropertySchema<any> = {
  fields: [],
  update: async () => {},
}

export function resolveSchema<T extends EntityType>(type: T): PropertySchema<EntityMap[T]> {
  return propertySchemas[type] ?? emptySchema
}
