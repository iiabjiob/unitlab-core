import type { SignalAllocationJob, SignalAllocationRow } from "@/types/signal"

function getSignalAllocationJobResult(job: SignalAllocationJob): Record<string, unknown> {
  return job.result && typeof job.result === "object" ? job.result : {}
}

export function getSignalAllocationJobResultNumber(job: SignalAllocationJob, key: string): number {
  const value = getSignalAllocationJobResult(job)[key]
  const numeric = Number(value)
  return Number.isFinite(numeric) ? Math.max(0, numeric) : 0
}

export function getSignalAllocationJobResultArrayLength(job: SignalAllocationJob, key: string): number {
  const value = getSignalAllocationJobResult(job)[key]
  return Array.isArray(value) ? value.length : 0
}

function hasOwnField(value: Record<string, unknown>, key: string): boolean {
  return Object.prototype.hasOwnProperty.call(value, key)
}

function readPatchField<T>(patch: Record<string, unknown>, key: string, fallback: T): T {
  if (!hasOwnField(patch, key)) {
    return fallback
  }
  const value = patch[key]
  return value === undefined ? fallback : value as T
}

export function normalizeSignalAllocationJobChangedRows(
  job: SignalAllocationJob,
  resolveBaseRow: (signalId: number) => SignalAllocationRow | null,
): SignalAllocationRow[] {
  const result = getSignalAllocationJobResult(job)
  const patchRows = result.changed_row_patches
  const changedRows = Array.isArray(patchRows) && patchRows.length > 0
    ? patchRows
    : result.changed_rows
  if (!Array.isArray(changedRows)) {
    return []
  }

  const rows: SignalAllocationRow[] = []
  changedRows.forEach((row) => {
    if (!row || typeof row !== "object") {
      return
    }
    const patch = row as Record<string, unknown>
    const signalId = Number(patch.signal_id)
    if (!Number.isFinite(signalId) || signalId <= 0) {
      return
    }

    const baseRow = resolveBaseRow(signalId)
    rows.push({
      ...(baseRow ?? {}),
      ...(patch as Partial<SignalAllocationRow>),
      signal_id: signalId,
      row_id: String(readPatchField(patch, "row_id", baseRow?.row_id ?? `signal-${signalId}`) ?? `signal-${signalId}`),
      signal_key: readPatchField(patch, "signal_key", baseRow?.signal_key ?? ""),
      signal_name: readPatchField(patch, "signal_name", baseRow?.signal_name ?? ""),
      signal_direction: readPatchField(
        patch,
        "signal_direction",
        (baseRow?.signal_direction ?? "DI") as SignalAllocationRow["signal_direction"],
      ),
      signal_category: readPatchField(patch, "signal_category", baseRow?.signal_category ?? null),
      signal_metadata: readPatchField(patch, "signal_metadata", baseRow?.signal_metadata ?? {}),
      allocation_id: readPatchField(patch, "allocation_id", baseRow?.allocation_id ?? null),
      allocation_status: readPatchField(patch, "allocation_status", baseRow?.allocation_status ?? "unassigned"),
      allocation_health: readPatchField(patch, "allocation_health", baseRow?.allocation_health ?? null),
      channel_id: readPatchField(patch, "channel_id", baseRow?.channel_id ?? null),
      channel_type: readPatchField(patch, "channel_type", baseRow?.channel_type ?? null),
      channel_index: readPatchField(patch, "channel_index", baseRow?.channel_index ?? null),
      channel_label: readPatchField(patch, "channel_label", baseRow?.channel_label ?? null),
      device_id: readPatchField(patch, "device_id", baseRow?.device_id ?? null),
      unit_id: readPatchField(patch, "unit_id", baseRow?.unit_id ?? null),
      unit_online: readPatchField(patch, "unit_online", baseRow?.unit_online ?? null),
      unit_last_seen_at: readPatchField(patch, "unit_last_seen_at", baseRow?.unit_last_seen_at ?? null),
      tested_at: readPatchField(patch, "tested_at", baseRow?.tested_at ?? null),
    })
  })
  return rows
}
