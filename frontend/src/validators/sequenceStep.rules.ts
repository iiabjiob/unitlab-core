import type { FieldRules } from "./core"
import { StepKind } from "@/types/sequences"
import { VALIDATION_LEVELS } from "./types"

export const sequenceStepRules: Partial<Record<StepKind, Record<string, FieldRules>>> = {
  [StepKind.WAIT]: {
    "payload.ms": { required: true, type: "number", min: 1, level: VALIDATION_LEVELS.ERROR },
  },

  [StepKind.DO_LATCH]: {
    "payload.ch": { required: true, level: VALIDATION_LEVELS.ERROR },
    "payload.value": { required: true, level: VALIDATION_LEVELS.ERROR },
  },

  [StepKind.DO_PULSE]: {
    "payload.ch": { required: true, level: VALIDATION_LEVELS.ERROR },
    "payload.value": { required: true, level: VALIDATION_LEVELS.ERROR },
    "payload.pulse_ms": { required: true, type: "number", min: 1, level: VALIDATION_LEVELS.ERROR },
  },

  [StepKind.DO_BITMASK]: {
    unit_id: { required: true, level: VALIDATION_LEVELS.ERROR },
    "payload.bitmask": { required: true, level: VALIDATION_LEVELS.ERROR },
  },

  [StepKind.DO_PAIR]: {
    "payload.chA": { required: true, notEqualTo: "payload.chB", level: VALIDATION_LEVELS.ERROR },
    "payload.chB": { required: true, level: VALIDATION_LEVELS.ERROR },
  },

  [StepKind.AO_SET]: {
    "payload.ch": { required: true, level: VALIDATION_LEVELS.ERROR },
    "payload.value": { required: true, type: "number", min: 4, max: 20, level: VALIDATION_LEVELS.WARNING },
  },
}
