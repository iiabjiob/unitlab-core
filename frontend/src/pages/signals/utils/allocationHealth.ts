import type { SignalAllocationRow } from "@/types/signal"

export type SignalAllocationQuickFilter =
  | "all"
  | "unassigned"
  | "assigned"
  | "issues"
  | "conflicts"
  | "invalid"
  | "offline_missing"

export const SIGNAL_ALLOCATION_QUICK_FILTERS: readonly SignalAllocationQuickFilter[] = [
  "all",
  "unassigned",
  "assigned",
  "issues",
  "conflicts",
  "invalid",
  "offline_missing",
]

export type SignalAllocationQuickFilterCounts = Record<SignalAllocationQuickFilter, number>

export function resolveSignalAllocationStatus(row: SignalAllocationRow): string {
  const status = String(row.allocation_status ?? "").trim().toLowerCase()
  if (status) {
    if (status === "allocated") return "assigned"
    if (status === "unallocated") return "unassigned"
    return status
  }
  return Number.isFinite(row.channel_id as number) ? "assigned" : "unassigned"
}

export function getSignalAllocationHealthFlag(row: SignalAllocationRow, key: string): boolean {
  const health = row.allocation_health
  if (!health || typeof health !== "object") {
    return false
  }
  return health[key] === true
}

export function hasSignalAllocationIssue(row: SignalAllocationRow): boolean {
  const status = resolveSignalAllocationStatus(row)
  if (status === "conflict" || status === "invalid" || status === "missing") {
    return true
  }
  return getSignalAllocationHealthFlag(row, "conflict")
    || getSignalAllocationHealthFlag(row, "invalid_type")
    || getSignalAllocationHealthFlag(row, "missing_device")
    || getSignalAllocationHealthFlag(row, "missing_channel")
    || getSignalAllocationHealthFlag(row, "offline_device")
    || getSignalAllocationHealthFlag(row, "stale_device")
}

export function resolveSignalAllocationHealthLabel(row: SignalAllocationRow): string {
  const status = resolveSignalAllocationStatus(row)
  if (status === "unassigned") return "Unassigned"
  if (getSignalAllocationHealthFlag(row, "conflict") || status === "conflict") return "Conflict"
  if (getSignalAllocationHealthFlag(row, "invalid_type") || status === "invalid") return "Invalid type"
  if (getSignalAllocationHealthFlag(row, "missing_device")) return "Missing device"
  if (getSignalAllocationHealthFlag(row, "missing_channel") || status === "missing") return "Missing channel"
  if (getSignalAllocationHealthFlag(row, "offline_device")) return "Offline"
  if (getSignalAllocationHealthFlag(row, "stale_device")) return "Stale"
  return "OK"
}

export function resolveSignalAllocationStatusLabel(row: SignalAllocationRow): string {
  const status = resolveSignalAllocationStatus(row)
  if (status === "assigned") return "Assigned"
  if (status === "unassigned") return "Unassigned"
  if (status === "conflict") return "Conflict"
  if (status === "invalid") return "Invalid"
  if (status === "missing") return "Missing"
  return status ? status.replace(/_/g, " ") : "-"
}

export function matchesSignalAllocationQuickFilter(
  row: SignalAllocationRow,
  filter: SignalAllocationQuickFilter,
): boolean {
  const status = resolveSignalAllocationStatus(row)
  if (filter === "all") return true
  if (filter === "unassigned") return status === "unassigned"
  if (filter === "assigned") return status === "assigned"
  if (filter === "issues") return hasSignalAllocationIssue(row)
  if (filter === "conflicts") {
    return status === "conflict" || getSignalAllocationHealthFlag(row, "conflict")
  }
  if (filter === "invalid") {
    return status === "invalid" || getSignalAllocationHealthFlag(row, "invalid_type")
  }
  if (filter === "offline_missing") {
    return status === "missing"
      || getSignalAllocationHealthFlag(row, "missing_device")
      || getSignalAllocationHealthFlag(row, "missing_channel")
      || getSignalAllocationHealthFlag(row, "offline_device")
      || getSignalAllocationHealthFlag(row, "stale_device")
  }
  return true
}

export function countSignalAllocationQuickFilters(
  rows: readonly SignalAllocationRow[],
): SignalAllocationQuickFilterCounts {
  const counts: SignalAllocationQuickFilterCounts = {
    all: rows.length,
    unassigned: 0,
    assigned: 0,
    issues: 0,
    conflicts: 0,
    invalid: 0,
    offline_missing: 0,
  }

  rows.forEach((row) => {
    SIGNAL_ALLOCATION_QUICK_FILTERS.forEach((filter) => {
      if (filter === "all") {
        return
      }
      if (matchesSignalAllocationQuickFilter(row, filter)) {
        counts[filter] += 1
      }
    })
  })

  return counts
}
