// validators/sequence.rules.ts
import type { FieldRules } from "./core"
import { VALIDATION_LEVELS } from "./types"

export const sequenceRules: Record<string, FieldRules> = {
  title: { required: true, level:VALIDATION_LEVELS.WARNING },
}
