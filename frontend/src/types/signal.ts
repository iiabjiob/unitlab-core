export interface SignalSnapshot {
  id: number
  workspace_id: number
  status: "draft" | "locked"
  source_filename: string | null
  source_hash: string | null
  rows_count: number
  schema_version: number
  data: Array<Record<string, unknown>>
  locked_at: string | null
  created_at: string
  updated_at: string
}

export interface SignalSnapshotSummary extends Omit<SignalSnapshot, "data"> {}

export interface AllocationMappingItem {
  channel_id: string
  signal_key: string
  signal_row_index: number
  meta?: Record<string, unknown> | null
}

export interface Allocation {
  id: number
  workspace_id: number
  signal_snapshot_id: number
  mapping: AllocationMappingItem[]
  created_at: string
  updated_at: string
}

export interface TestRun {
  id: number
  workspace_id: number
  sequence_id: number
  signal_snapshot_id: number
  allocation_snapshot: AllocationMappingItem[]
  status: "created" | "running" | "completed" | "failed"
  created_at: string
  started_at: string | null
  finished_at: string | null
  execution_meta?: Record<string, unknown> | null
}

export interface TestRunCreatePayload {
  sequence_id: number
  signal_snapshot_id: number
}
