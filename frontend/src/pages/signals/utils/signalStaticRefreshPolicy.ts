export type SignalStaticRefreshReason =
  | "initial_load"
  | "workspace_switch"
  | "import"
  | "operator_manual_refresh"
  | "reconnect_gap"
  | "unknown_row_patch"

export type SignalStaticRefreshTrigger =
  | { kind: "initial_load" }
  | { kind: "workspace_switch" }
  | { kind: "import" }
  | { kind: "operator_manual_refresh" }
  | {
    kind: "signal_rows_patched"
    requiresFullReload?: boolean
    missingSignalIds?: readonly unknown[]
  }
  | { kind: "allocation_job_completed" }
  | { kind: "runtime_patch_burst" }

function hasMissingSignalIds(values: readonly unknown[] | undefined): boolean {
  return Array.isArray(values) && values.some((value) => {
    const signalId = Number(value)
    return Number.isFinite(signalId) && signalId > 0
  })
}

export function resolveSignalStaticRefreshReason(
  trigger: SignalStaticRefreshTrigger,
): SignalStaticRefreshReason | null {
  switch (trigger.kind) {
    case "initial_load":
      return "initial_load"
    case "workspace_switch":
      return "workspace_switch"
    case "import":
      return "import"
    case "operator_manual_refresh":
      return "operator_manual_refresh"
    case "signal_rows_patched":
      if (trigger.requiresFullReload === true) {
        return "reconnect_gap"
      }
      if (hasMissingSignalIds(trigger.missingSignalIds)) {
        return "unknown_row_patch"
      }
      return null
    case "allocation_job_completed":
    case "runtime_patch_burst":
      return null
    default:
      return null
  }
}
