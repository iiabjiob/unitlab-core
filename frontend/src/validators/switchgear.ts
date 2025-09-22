// validators/switchgear.ts
import { validateByRules } from "./core"
import { switchgearRules } from "./switchgear.rules"
import type { Switchgear } from "@/types/switchgear"
import type { ValidationError } from "./types"
import { SCHEMA_NAMES } from "@/property-schemas/types"

export function validateSwitchgear(item: Switchgear): ValidationError[] {
  return validateByRules(item, SCHEMA_NAMES.SWITCHGEAR, switchgearRules)
}
