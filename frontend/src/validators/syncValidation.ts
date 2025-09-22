// src/validators/syncValidation.ts
import { useValidationStore } from "@/stores/validationStore"
import type { SchemaName } from "@/property-schemas/types"
import type { ValidationError } from "@/validators/types"

export type ValidatorFn<T> = (item: T) => ValidationError[]

/**
 * Revalidate a single item and update ValidationStore
 */
export function validateOne<T>(
  schemaName: SchemaName,
  item: T,
  validator: ValidatorFn<T>
) {
  const vStore = useValidationStore()
  const errs = validator(item)
  vStore.replaceItemErrors(schemaName, (item as any).id, errs)
}

/**
 * Remove all errors for deleted item
 */
export function clearOne(schemaName: SchemaName, itemId: string | number) {
  const vStore = useValidationStore()
  vStore.replaceItemErrors(schemaName, itemId, [])
}

/**
 * Run validation for a list of items
 */
export function validateAll<T>(
  schemaName: SchemaName,
  items: T[],
  validator: ValidatorFn<T>
) {
  const vStore = useValidationStore()
  for (const item of items) {
    const errs = validator(item)
    vStore.replaceItemErrors(schemaName, (item as any).id, errs)
  }
}
