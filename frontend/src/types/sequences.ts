export enum StepKind {
  WAIT = "WAIT",
  DO_SET = "DO_SET",
  DO_RESET_ALL = "DO_RESET_ALL",
  AO_SET = "AO_SET",
}

export type SequenceStatus = "idle" | "running" | "stopped" | "completed"

export interface SequenceStep {
  id?: number
  order_index: number
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
