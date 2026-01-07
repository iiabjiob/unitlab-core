import type { ChannelDto } from "./channel"

export enum TestRunStatusEnum {
  PENDING = "pending",
  RUNNING = "running",
  COMPLETED = "completed",
  FAILED = "failed",
  CANCELLED = "cancelled",
}

export interface TestRunSettings {
  delay_ms: number
}

export interface TestRunStep {
  id: number
  test_run_id: number
  order_index: number
  channel_id: number | null
  channel?: ChannelDto | null
  created_at: string
  updated_at: string
}

export interface TestRunSummary {
  id: number
  project_id: number
  name: string
  status: TestRunStatusEnum
  settings: TestRunSettings
  current_step_index: number
  error_message: string | null
  created_at: string
  updated_at: string
  started_at: string | null
  finished_at: string | null
}

export interface TestRunSchema extends TestRunSummary {
  steps?: TestRunStep[]
}

export interface TestRunState {
  id: number
  status: TestRunStatusEnum
  started_at: string | null
  finished_at: string | null
  current_step_index: number
  total_steps: number
  error_message: string | null
}

export interface TestRunCreatePayload {
  name: string
  channel_ids: number[]
  settings?: Partial<TestRunSettings>
}

export interface TestRunUpdatePayload {
  name?: string
  settings?: Partial<TestRunSettings>
}
