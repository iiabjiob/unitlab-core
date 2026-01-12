export interface SnapshotSheet {
  name: string
  index: number
  headers: string[]
  rows_count: number
  rows: Array<Record<string, unknown>>
}

export type InternalSignalType = "di" | "do" | "ao" | "ai"

export interface SignalSnapshotDataV2 {
  version: number
  sheet_count: number
  default_sheet_index: number
  sheets: SnapshotSheet[]
}

export type SignalSnapshotData = Array<Record<string, unknown>> | SignalSnapshotDataV2

export interface SignalSnapshot {
  id: number
  workspace_id: number
  status: "draft" | "locked"
  source_filename: string | null
  source_hash: string | null
  rows_count: number
  schema_version: number
  data: SignalSnapshotData
  locked_at: string | null
  created_at: string
  updated_at: string
}

export interface SignalSnapshotSummary extends Omit<SignalSnapshot, "data"> {}

export interface AllocationMappingMeta {
  sheet_index: number | null
  sheet_name?: string | null
  column_key?: string | null
}

export interface AllocationMappingItem {
  channel_id: string
  signal_key: string
  signal_row_index: number
  meta?: AllocationMappingMeta | null
}

export interface SignalImportMeta {
  sheet_name: string
  source_sheet_name?: string | null
  hmi_representation?: string | null
  type_column?: string | null
  type_mapping?: Record<string, InternalSignalType>
  internal_type_column?: string | null
}

export interface Allocation {
  id: number
  workspace_id: number
  signal_snapshot_id: number
  mapping: AllocationMappingItem[]
  created_at: string
  updated_at: string
}

export type TestRunMode = "channel" | "signal"

export interface TestRunAllocationEntry {
  id: number
  channel_id: number
  signal_key: string | null
  signal_metadata: Record<string, unknown> | null
}

export interface TestRunAllocation {
  id: number
  test_run_id: number
  notes: string | null
  created_at: string
  updated_at: string
  entries: TestRunAllocationEntry[]
}

export interface TestRun {
  id: number
  workspace_id: number
  signal_snapshot_id: number | null
  allocation: TestRunAllocation | null
  sequence_ids: number[]
  mode: TestRunMode
  status: "created" | "running" | "completed" | "failed"
  created_at: string
  started_at: string | null
  finished_at: string | null
  execution_meta?: Record<string, unknown> | null
}

export interface TestRunAllocationEntryInput {
  channel_id: number
  signal_key?: string | null
  signal_metadata?: Record<string, unknown> | null
}

export interface TestRunAllocationCreatePayload {
  notes?: string | null
  entries: TestRunAllocationEntryInput[]
}

export interface TestRunCreatePayload {
  workspace_id: number
  sequence_ids: number[]
  allocation: TestRunAllocationCreatePayload
  mode: TestRunMode
  signal_snapshot_id?: number | null
}
