// utils/updateEntity.ts
import { resolveSchema} from "@/property-schemas/propertySchemas"
import type { EntityMap, EntityType } from "@/types/entity"

export async function updateEntity<T extends EntityType, K extends keyof EntityMap[T]>(
  type: T,
  entity: EntityMap[T],
  key: K,
  value: EntityMap[T][K]
) {
  const schema = resolveSchema(type)
  await schema.update(entity, key, value)
}
