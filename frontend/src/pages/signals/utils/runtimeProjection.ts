import type { SignalAllocationRow } from "@/types/signal"

type TestedAtResolver = (
  signalId: number | null | undefined,
  workspaceId?: number | null,
) => string | null

export function resolveRuntimeTestedAt(
  row: SignalAllocationRow,
  workspaceId: number | null | undefined,
  getTestedAt: TestedAtResolver,
): string | null {
  const patchedTestedAt = String(getTestedAt(row.signal_id, workspaceId) ?? "").trim()
  return patchedTestedAt || row.tested_at
}

export function applyRuntimeTestedAt(
  row: SignalAllocationRow,
  workspaceId: number | null | undefined,
  getTestedAt: TestedAtResolver,
): SignalAllocationRow {
  const testedAt = resolveRuntimeTestedAt(row, workspaceId, getTestedAt)
  if (testedAt === row.tested_at) {
    return row
  }
  return {
    ...row,
    tested_at: testedAt,
  }
}

export function applyRuntimeTestedAtToRows(
  rows: readonly SignalAllocationRow[],
  workspaceId: number | null | undefined,
  getTestedAt: TestedAtResolver,
): SignalAllocationRow[] {
  return rows.map(row => applyRuntimeTestedAt(row, workspaceId, getTestedAt))
}
