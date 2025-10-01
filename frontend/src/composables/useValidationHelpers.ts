import type { SchemaName } from "@/property-schemas/types"
import { useValidationStore } from "@/stores/validationStore"
import { VALIDATION_LEVELS } from "@/validators/types"

export function useValidationHelpers() {
  const store = useValidationStore()

  function getErrors(schemaName: SchemaName, itemId: number | string, fieldKey?: string) {
    return store.errors.filter(e =>
      e.schemaName === schemaName &&
      e.itemId === itemId &&
      (fieldKey ? e.fieldKey === fieldKey : true)
    )
  }

  function getItemErrors(schemaName: SchemaName, itemId: number | string) {
    return store.errors.filter(e =>
      e.schemaName === schemaName && e.itemId === itemId
    )
  }

  function hasError(schemaName: SchemaName, itemId: number | string, fieldKey?: string) {
    return getErrors(schemaName, itemId, fieldKey).length > 0
  }

  function validationClass(schemaName: SchemaName, itemId: number | string, fieldKey?: string) {
    const errs = getErrors(schemaName, itemId, fieldKey)
    if (!errs.length) return ""

    if (errs.some(e => e.level === VALIDATION_LEVELS.ERROR)) return "validation-error"
    if (errs.some(e => e.level === VALIDATION_LEVELS.WARNING)) return "validation-warning"
    return ""
  }

  return {
    getErrors,
    getItemErrors,
    hasError,
    validationClass
  }
}
