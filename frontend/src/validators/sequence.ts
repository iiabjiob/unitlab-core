// validators/sequence.ts
import { validateByRules } from "./core"
import { sequenceRules } from "./sequence.rules"
import type { SequenceDef } from "@/types/sequences"
import type { ValidationError } from "./types"
import { SCHEMA_NAMES } from "@/property-schemas/types"

export function validateSequence(item: SequenceDef): ValidationError[] {
  return validateByRules(item, SCHEMA_NAMES.SEQUENCE, sequenceRules)
}
