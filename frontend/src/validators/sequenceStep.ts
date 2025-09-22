// validators/sequenceStep.ts
import { validateByRules } from "./core"
import { sequenceStepRules } from "./sequenceStep.rules"
import type { SequenceStep } from "@/types/sequences"
import type { ValidationError } from "./types"
import { SCHEMA_NAMES } from "@/property-schemas/types"

export function validateSequinceStep(item: SequenceStep): ValidationError[] {
  const rules = sequenceStepRules[item.kind]
  if (!rules) return []

  return validateByRules(item, SCHEMA_NAMES.SEQUENCE_STEP, rules)
}
