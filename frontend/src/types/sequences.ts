export enum SequenceStepType {
  WAIT = "WAIT",
  DO_LATCH = "DO_LATCH",
  DO_PULSE = "DO_PULSE",
  DO_PAIR = "DO_PAIR",
  DO_BITMASK = "DO_BITMASK",
  AO_SET = "AO_SET",
}

export enum SequenceStatusEnum {
  IDLE = "idle",
  PENDING = "pending",
  RUNNING = "running",
  CANCELLING = "cancelling",
  STOPPED = "stopped",
  COMPLETED = "completed",
  ERROR = "error",
}

export enum SequenceRunStatusEnum {
  PENDING = "pending",
  RUNNING = "running",
  CANCELLING = "cancelling",
  COMPLETED = "completed",
  STOPPED = "stopped",
  ERROR = "error",
}

export enum SequenceRunStepStatusEnum {
  PENDING = "pending",
  RUNNING = "running",
  COMPLETED = "completed",
  ERROR = "error",
  CANCELLED = "cancelled",
}

export interface SequenceStepPayload {
  device_id?: number
  ms?: number
  value?: number
  bitmask?: number
  state2b?: number
  pulse_ms?: number
  channel_ids?: number[]
}

export interface SequenceStep {
  id: number
  sequence_id: number
  order_index: number
  sequence_step_type: SequenceStepType
  channel_id?: number | null
  payload?: SequenceStepPayload | null
  created_at: string
  updated_at: string
}

export interface SequenceStepCreate {
  sequence_step_type: SequenceStepType
  channel_id?: number | null
  payload?: Record<string, any> | null
}

export interface SequenceDef {
  id: number
  workspace_ids: number[]
  name: string
  description?: string | null
  system_key?: string | null
  system_provided: boolean
  read_only: boolean
  created_at: string
  updated_at: string
  steps?: SequenceStep[]
}

export interface SequenceRunStep {
  id: number
  run_id: number
  sequence_step_id: number
  order_index: number
  status: SequenceRunStepStatusEnum
  started_at?: string | null
  finished_at?: string | null
  error_message?: string | null
  elapsed_ms?: number | null
}

export interface SequenceRun {
  id: number
  sequence_id: number
  status: SequenceRunStatusEnum
  started_at: string
  finished_at?: string | null
  error_message?: string | null
  current_step_index: number
  steps: SequenceRunStep[]
}

export interface SequenceState {
  sequence_id: number
  status: SequenceStatusEnum
  run_id?: number | null
  current_step_index: number
  total_steps: number
  completed_step_ids: number[]
  last_error?: string | null
  started_at?: string | null
  finished_at?: string | null
}
