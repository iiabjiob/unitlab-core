// config/propertySchemas.ts
import { devicePropertySchema } from "./device.schema"
import type { PropertySchema } from "@/property-schemas/types"
import { switchgearPropertySchema } from "./switchgear.schema"
import type { EntityMap, EntityType } from "@/types/entity"
import { channelPropertySchema } from "./channel.schema"
import { sequencePropertySchema } from "./sequence.schema"
import { sequenceStepPropertySchema } from "./sequenceStep.schema"


export const propertySchemas: {
  [K in EntityType]: PropertySchema<EntityMap[K]>
} = {
  channel: channelPropertySchema,
  device: devicePropertySchema,
  switchgear: switchgearPropertySchema,
  sequence: sequencePropertySchema,
  sequence_step: sequenceStepPropertySchema,
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
