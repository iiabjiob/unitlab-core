// validators/core.ts
import type { SchemaName } from "@/property-schemas/types"
import { VALIDATION_LEVELS, type ValidationError, type ValidationLevel } from "./types"
import { getValueByPath } from "@/utils/object"

export interface FieldRule {
  required?: boolean
  recommended?: boolean
  min?: number
  max?: number
  pattern?: RegExp
  notEqualTo?: string
  type?: "number" | "string"
  level: ValidationLevel
  message?: string
}

export type FieldRules = FieldRule | FieldRule[]

export function validateByRules<T>(
  item: T,
  schemaName: SchemaName,
  rules: Record<string, FieldRules>
): ValidationError[] {
  const errs: ValidationError[] = []

  for (const [field, fieldRules] of Object.entries(rules)) {
    const value = getValueByPath(item, field)
    const ruleList = Array.isArray(fieldRules) ? fieldRules : [fieldRules]

    for (const rule of ruleList) {
      // required
      if (rule.required && (value === null || value === undefined || value === "")) {
        errs.push({
          schemaName,
          itemId: (item as any).id,
          fieldKey: field,
          message: rule.message ?? `${field} is required`,
          level: rule.level ?? VALIDATION_LEVELS.ERROR
        })
        continue
      }

      // recommended
      if (rule.recommended && (value === null || value === undefined || value === "")) {
        errs.push({
          schemaName,
          itemId: (item as any).id,
          fieldKey: field,
          message: rule.message ?? `${field} is recommended`,
          level: rule.level ?? "warning"
        })
        continue
      }

      // type check
      if (rule.type === "number") {
        if (typeof value !== "number" || isNaN(value)) {
          errs.push({
            schemaName,
            itemId: (item as any).id,
            fieldKey: field,
            message: rule.message ?? `${field} must be a valid number`,
            level: rule.level ?? VALIDATION_LEVELS.ERROR
          })
          continue
        }
      }

      // min / max
      if (typeof value === "number") {
        if (rule.min !== undefined && value < rule.min) {
          errs.push({
            schemaName,
            itemId: (item as any).id,
            fieldKey: field,
            message: rule.message ?? `${field} must be >= ${rule.min}`,
            level: rule.level ?? VALIDATION_LEVELS.ERROR
          })
        }
        if (rule.max !== undefined && value > rule.max) {
          errs.push({
            schemaName,
            itemId: (item as any).id,
            fieldKey: field,
            message: rule.message ?? `${field} must be <= ${rule.max}`,
            level: rule.level ?? VALIDATION_LEVELS.ERROR
          })
        }
      }

      // pattern
      if (rule.pattern && typeof value === "string" && !rule.pattern.test(value)) {
        errs.push({
          schemaName,
          itemId: (item as any).id,
          fieldKey: field,
          message: rule.message ?? `${field} format is invalid`,
          level: rule.level ?? VALIDATION_LEVELS.ERROR
        })
      }

      // notEqualTo
      if (rule.notEqualTo) {
        const other = getValueByPath(item, rule.notEqualTo)
        if (value && other && value === other) {
          errs.push({
            schemaName,
            itemId: (item as any).id,
            fieldKey: field,
            message: rule.message ?? `${field} cannot equal ${rule.notEqualTo}`,
            level: VALIDATION_LEVELS.ERROR
          })
        }
      }
    }
  }

  return errs
}
