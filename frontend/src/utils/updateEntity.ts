// utils/updateEntity.ts
import { type EntityMap, type EntityType, resolveSchema} from "@/property-schemas/propertySchemas"

export async function updateEntity<T extends EntityType, K extends keyof EntityMap[T]>(
  type: T,
  entity: EntityMap[T],
  key: K,
  value: EntityMap[T][K]
) {
  const schema = resolveSchema(type)
  await schema.update(entity, key, value)
}
