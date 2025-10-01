export enum StepKind {
  WAIT = "WAIT",
  AO_SET = "AO_SET",

  DO_LATCH = "DO_LATCH",     // single bit
  DO_PULSE = "DO_PULSE",     // pulse
  DO_PAIR = "DO_PAIR",       // 2-bit state
  DO_BITMASK = "DO_BITMASK", // all bitmask
}

export enum SequenceStatusEnum {
  IDLE = "idle",
  RUNNING = "running",
  STOPPED = "stopped",
  COMPLETED = "completed",
}

// Тип: только значения enum
export type SequenceStatus = `${SequenceStatusEnum}`


export interface SequenceStepPayload {

  device_id?: number

  // WAIT
  ms?: number

  // DO
  value?: number
  bitmask?: number
  state2b?: number
  pulse_ms?: number

  channel_ids?: number[]
  // AO
  // value?: number уже есть
}

export interface SequenceStep {
  id: number
  sequence_id: number
  order_index: number
  kind: StepKind

  channel_id?: number | null
  device_id?: number | null
  payload?: SequenceStepPayload | null
}

export interface SequenceStepCreate {
  kind: StepKind
  channel_id?: number | null
  payload?: Record<string, any> | null
}

export interface SequenceDef {
  id: number
  name: string
  description?: string | null
}
