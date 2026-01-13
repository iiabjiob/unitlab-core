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

// --- Live signal domain (in-progress migration) ---

export type SignalIODirection = "DI" | "DO" | "AI" | "AO"

export interface Signal {
  id: number
  workspace_id: number
  key: string
  name: string
  io_direction: SignalIODirection
  category: string | null
  signal_metadata: Record<string, unknown>
  is_active: boolean
  deleted_at: string | null
  created_at: string
  updated_at: string
}

export interface SignalCreatePayload {
  key: string
  name: string
  io_direction: SignalIODirection
  category?: string | null
  metadata?: Record<string, unknown>
  is_active?: boolean
}

export interface SignalUpdatePayload {
  name?: string | null
  io_direction?: SignalIODirection | null
  category?: string | null
  metadata?: Record<string, unknown> | null
  is_active?: boolean | null
}

export interface TestRunSignalSnapshotSummary {
  test_run_id: number
  workspace_id: number
  captured_at: string
}

export interface TestRunSignalSnapshotEntry {
  id: number
  snapshot_id: number
  live_signal_id: number | null
  signal_key: string
  name: string
  io_direction: SignalIODirection
  allocation_channel_id: number | null
  allocation_metadata: Record<string, unknown> | null
  entry_metadata: Record<string, unknown>
  created_at: string
}

export interface TestRunSignalSnapshot extends TestRunSignalSnapshotSummary {
  entries: TestRunSignalSnapshotEntry[]
}

export type TestRunStatus = "created" | "running" | "completed" | "failed"

export interface AllocationEntry {
  id: number
  channel_id: number
  signal_id: number | null
  signal_key: string | null
  signal_metadata: Record<string, unknown> | null
}

export interface AllocationRecord {
  id: number
  test_run_id: number
  notes: string | null
  created_at: string
  updated_at: string
  entries: AllocationEntry[]
}

export interface AllocationEntryInput {
  channel_id: number
  signal_id?: number | null
  signal_metadata?: Record<string, unknown> | null
}

export interface AllocationCreatePayload {
  notes?: string | null
  entries?: AllocationEntryInput[]
}

export interface TestRunRecord {
  id: number
  workspace_id: number
  allocation: AllocationRecord | null
  sequence_ids: number[]
  status: TestRunStatus
  created_at: string
  started_at: string | null
  finished_at: string | null
  execution_meta: Record<string, unknown> | null
  snapshot: TestRunSignalSnapshotSummary | null
}

export interface TestRunCreatePayloadV2 {
  workspace_id: number
  sequence_ids: number[]
  allocation?: AllocationCreatePayload
  allow_empty_allocation?: boolean
}
