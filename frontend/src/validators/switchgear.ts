// validators/switchgear.ts
import { validateByRules } from "./core"
import { switchgearRules } from "./switchgear.rules"
import type { Switchgear } from "@/types/switchgear"
import type { ValidationError } from "./types"
import { SCHEMA_NAMES } from "@/property-schemas/types"

export function validateSwitchgear(item: Switchgear): ValidationError[] {
  return validateByRules(item, SCHEMA_NAMES.SWITCHGEAR, switchgearRules)
}

export function validateSwitchgearField(item: Switchgear, key: keyof Switchgear): ValidationError[] {
  const rules = (switchgearRules as any)[key]
  if (!rules) return []
  return validateByRules(item, SCHEMA_NAMES.SWITCHGEAR, { [key]: rules })
}
