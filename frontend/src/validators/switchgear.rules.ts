// validators/switchgear.rules.ts
import type { FieldRules } from "./core"
import { VALIDATION_LEVELS } from "./types"

export const switchgearRules: Record<string, FieldRules> = {
  title: { required: true, level:VALIDATION_LEVELS.WARNING },
  do_open: { required: true, notEqualTo: "do_closed", level: VALIDATION_LEVELS.ERROR },
  do_closed: { required: true, level: VALIDATION_LEVELS.ERROR },
  di_open: { recommended: true, level: VALIDATION_LEVELS.WARNING },
  di_closed: { recommended: true, level: VALIDATION_LEVELS.WARNING },

  feedback_delay_ms: [
    { min: 0, max: 10000, type: "number", level: VALIDATION_LEVELS.ERROR, message: "Feedback delay is incorrect" },
  ],
}
