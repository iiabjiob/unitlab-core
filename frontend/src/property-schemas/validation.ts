import type { PropertySchema, PropertyField } from "./types"

export type ValidationLevel = "error" | "warning"

export interface ValidationError {
  itemId: string | number
  schemaName: string
  fieldKey: string
  message: string
  level?: ValidationLevel
}

function getLabel<T>(field: PropertyField<T>, item: T): string {
  if (typeof field.label === "function") {
    return field.label(item)
  }
  return field.label
}

export function validateItem<T>(
  schema: PropertySchema<T>,
  item: T,
  schemaName: string
): ValidationError[] {
  const errors: ValidationError[] = []

  for (const field of schema.fields) {
    if (field.visible && !field.visible(item)) continue

    const value = (item as any)[field.key]

    // required
    if ("required" in field && field.required) {
      if (value === null || value === undefined || value === "") {
        errors.push({
          itemId: (item as any).id ?? "?",
          schemaName,
          fieldKey: field.key,
          message: `${getLabel(field, item)} is required`,
          level: "error"
        })
        continue
      }
    }

    // custom validation
    if ("validate" in field && typeof field.validate === "function") {
      const msg = field.validate(value, item)
      if (msg) {
        errors.push({
          itemId: (item as any).id ?? "?",
          schemaName,
          fieldKey: field.key,
          message: msg,
          level: "error"
        })
      }
    }
  }

  return errors
}
