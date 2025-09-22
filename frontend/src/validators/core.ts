// validators/core.ts
import type { SchemaName } from "@/property-schemas/types"
import type { ValidationError, ValidationLevel } from "./types"

export interface FieldRules {
  required?: boolean
  min?: number
  max?: number
  pattern?: RegExp
  notEqualTo?: string
  type?: "number" | "string"
  level?: ValidationLevel
}

export function validateByRules<T>(
  item: T,
  schemaName: SchemaName,
  rules: Record<string, FieldRules>
): ValidationError[] {
  const errs: ValidationError[] = []

  for (const [field, rule] of Object.entries(rules)) {
    const value = (item as any)[field]

    // required
    if (rule.required && (value === null || value === undefined || value === "")) {
      errs.push({
        schemaName,
        itemId: (item as any).id,
        fieldKey: field,
        message: `${field} is required`,
        level: rule.level ?? "error"
      })
      continue
    }

    // min / max
    if (typeof value === "number") {
      if (rule.min !== undefined && value < rule.min) {
        errs.push({
          schemaName,
          itemId: (item as any).id,
          fieldKey: field,
          message: `${field} must be >= ${rule.min}`,
          level: rule.level ?? "error"
        })
      }
      if (rule.max !== undefined && value > rule.max) {
        errs.push({
          schemaName,
          itemId: (item as any).id,
          fieldKey: field,
          message: `${field} must be <= ${rule.max}`,
          level: rule.level ?? "error"
        })
      }
    }

    // pattern
    if (rule.pattern && typeof value === "string" && !rule.pattern.test(value)) {
      errs.push({
        schemaName,
        itemId: (item as any).id,
        fieldKey: field,
        message: `${field} format is invalid`,
        level: rule.level ?? "error"
      })
    }

    // notEqualTo
    if (rule.notEqualTo) {
      const other = (item as any)[rule.notEqualTo]
      if (value && other && value === other) {
        errs.push({
          schemaName,
          itemId: (item as any).id,
          fieldKey: field,
          message: `${field} cannot equal ${rule.notEqualTo}`,
          level: "error"
        })
      }
    }

    // type check
    if (rule.type === "number") {
      if (typeof value !== "number" || isNaN(value)) {
        errs.push({
          schemaName,
          itemId: (item as any).id,
          fieldKey: field,
          message: `${field} must be a valid number`,
          level: rule.level ?? "error"
        })
        continue
      }
    }

  }

  return errs
}
