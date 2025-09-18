export enum StepKind {
  WAIT = "WAIT",
  AO_SET = "AO_SET",

  DO_LATCH = "DO_LATCH",     // single bit
  DO_PULSE = "DO_PULSE",     // pulse
  DO_PAIR = "DO_PAIR",       // 2-bit state
  DO_BITMASK = "DO_BITMASK", // all bitmask
}

export type SequenceStatus = "idle" | "running" | "stopped" | "completed"


export interface SequenceStepPayload {
  // WAIT
  ms?: number

  // DO_SET / DO_RESET_ALL
  mode?: number
  ch?: number
  value?: number
  bitmask?: number

  // расширенные DO команды
  chA?: number
  chB?: number
  state2b?: number
  pulse_ms?: number

  // AO_SET
  // ch и value уже есть
}

export interface SequenceStep {
  id: number
  sequence_id: number
  order_index: number
  kind: StepKind
  unit_id?: string | null
  payload?: SequenceStepPayload | null
}

export interface SequenceStepCreate {
  kind: StepKind
  unit_id?: string | null
  payload?: Record<string, any> | null
}

export interface SequenceDef {
  id: number
  name: string
  description?: string | null
  steps: SequenceStep[]
}
