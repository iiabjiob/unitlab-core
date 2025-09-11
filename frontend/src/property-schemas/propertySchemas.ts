// config/propertySchemas.ts
import { devicePropertySchema } from "./devicePropertySchema"
import type { PropertySchema } from "@/types/propertySchema"
import { switchgearPropertySchema } from "./switchgearPropertySchema"
import type { EntityMap, EntityType } from "@/types/entity"
import { channelPropertySchema } from "./channelPropertySchema"


export const propertySchemas: {
  [K in EntityType]: PropertySchema<EntityMap[K]>
} = {
  channel: channelPropertySchema,
  device: devicePropertySchema,
  switchgear: switchgearPropertySchema,
}

// fallback для неизвестного типа
export const emptySchema: PropertySchema<any> = {
  fields: [],
  update: async () => {},
}

export function resolveSchema<T extends EntityType>(
  type: T
): PropertySchema<EntityMap[T]> {
  return propertySchemas[type] ?? emptySchema
}
