import type { SchemaName } from "@/property-schemas/types"

export type ValidationLevel = "error" | "warning" | "info"

export interface ValidationError {
  itemId: string | number
  schemaName: SchemaName
  fieldKey: string
  message: string
  level: ValidationLevel
}
