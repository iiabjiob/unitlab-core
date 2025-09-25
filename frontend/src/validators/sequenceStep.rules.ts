import type { FieldRules } from "./core"
import { StepKind } from "@/types/sequences"
import { VALIDATION_LEVELS } from "./types"

export const sequenceStepRules: Partial<Record<StepKind, Record<string, FieldRules>>> = {
  [StepKind.WAIT]: {
    "payload.ms": { required: true, type: "number", min: 1, level: VALIDATION_LEVELS.ERROR },
  },

  [StepKind.DO_LATCH]: {
    "channel_id": { required: true, level: VALIDATION_LEVELS.ERROR },
    "payload.value": { required: true, level: VALIDATION_LEVELS.ERROR },
  },

  [StepKind.DO_PULSE]: {
    "channel_id": { required: true, level: VALIDATION_LEVELS.ERROR },
    "payload.value": { required: true, level: VALIDATION_LEVELS.ERROR },
    "payload.pulse_ms": { required: true, type: "number", min: 1, level: VALIDATION_LEVELS.ERROR },
  },

  [StepKind.DO_BITMASK]: {
    "payload.device_id": { required: true, level: VALIDATION_LEVELS.ERROR },
    "payload.bitmask": { required: true, level: VALIDATION_LEVELS.ERROR },
  },

  [StepKind.DO_PAIR]: {
    // "payload.channel_ids": {
    //   required: true,
    //   type: "array",
    //   minLength: 2,
    //   maxLength: 2,
    //   unique: true, // 👈 нужно будет реализовать в валидаторе "все элементы разные"
    //   level: VALIDATION_LEVELS.ERROR,
    // },
    // "payload.state2b": {
    //   required: true,
    //   type: "number",
    //   min: 0,
    //   max: 3, // у тебя 2-битное состояние
    //   level: VALIDATION_LEVELS.ERROR,
    // },
  },

  [StepKind.AO_SET]: {
    "channel_id": { required: true, level: VALIDATION_LEVELS.ERROR },
    "payload.value": { required: true, type: "number", min: 4, max: 20, level: VALIDATION_LEVELS.WARNING },
  },
}
