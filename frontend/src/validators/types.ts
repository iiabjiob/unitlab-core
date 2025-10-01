import type { SchemaName } from "@/property-schemas/types"

export const VALIDATION_LEVELS = {
  ERROR: "error",
  WARNING: "warning",
  INFO: "info",
} as const

export type ValidationLevel = typeof VALIDATION_LEVELS[keyof typeof VALIDATION_LEVELS]

export interface ValidationError {
  itemId: string | number
  schemaName: SchemaName
  fieldKey: string
  message: string
  level: ValidationLevel
}
