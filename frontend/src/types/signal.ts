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
  selected_columns?: string[]
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

export interface SignalSheet {
  id: number
  workspace_id: number
  source_filename: string | null
  source_hash: string | null
  rows_count: number
  schema_version: number
  data: Record<string, unknown>
  import_meta: SignalImportMeta | null
  signals_count: number
  allocated_count: number
  created_at: string
  updated_at: string
}

export interface SignalSheetImportResponse {
  sheet: SignalSheet
}

export interface SignalSheetPreset {
  id: number
  workspace_id: number
  name: string
  import_meta: SignalImportMeta
  created_at: string
  updated_at: string
}

export interface SignalAllocationRow {
  signal_id: number
  signal_key: string
  signal_name: string
  signal_direction: SignalIODirection
  signal_category: string | null
  signal_metadata: Record<string, unknown>
  channel_id: number | null
  channel_type: string | null
  channel_index: number | null
  channel_label: string | null
  device_id: number | null
  unit_id: string | null
  unit_online: boolean | null
  unit_last_seen_at: string | null
  tested_at: string | null
}

export interface SignalAllocationUpdateItem {
  signal_id: number
  channel_id?: number | null
  allocation_meta?: Record<string, unknown> | null
}

export interface SignalAutoAllocatePayload {
  signal_ids?: number[]
  prefer_online?: boolean
  overwrite_existing?: boolean
}

export interface SignalAutoAllocateResult {
  assigned: number
  skipped: number
  missing: number
  unassigned_signal_ids: number[]
}

export interface SignalAutoAllocateResponse {
  result: SignalAutoAllocateResult
  rows: SignalAllocationRow[]
}

export interface SignalAllocationEnsurePayload {
  signal_ids: number[]
  prefer_online?: boolean
}

export interface SignalAllocationEnsureResponse {
  result: SignalAutoAllocateResult
  rows: SignalAllocationRow[]
}
