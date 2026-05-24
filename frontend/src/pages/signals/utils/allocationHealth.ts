import type { SignalAllocationRow } from "@/types/signal"

export function resolveSignalAllocationStatus(row: SignalAllocationRow): string {
  const status = String(row.allocation_status ?? "").trim().toLowerCase()
  if (status) {
    if (status === "allocated") return "assigned"
    if (status === "unallocated") return "unassigned"
    return status
  }
  return Number.isFinite(row.channel_id as number) ? "assigned" : "unassigned"
}

function getSignalAllocationHealthFlag(row: SignalAllocationRow, key: string): boolean {
  const health = row.allocation_health
  if (!health || typeof health !== "object") {
    return false
  }
  return health[key] === true
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
