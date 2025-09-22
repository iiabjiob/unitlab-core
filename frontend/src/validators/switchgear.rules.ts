// validators/switchgear.rules.ts
import type { FieldRules } from "./core"

export const switchgearRules: Record<string, FieldRules> = {
  title: { required: true, level:"warning" },
  do_open: { required: true, notEqualTo: "do_closed" },
  do_closed: { required: true },
  feedback_delay_ms: {
    required: true,
    type: "number",
    max: 10000,
    level: "warning"
  },
}
