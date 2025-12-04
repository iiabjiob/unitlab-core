import type { UiTableColumn } from "../../../core/types"
import type { UseTableViewportOptions } from "../../composables/useTableViewport"
import type { ColumnPinMode } from "../../../core/virtualization/types"

export function resolveRowHeightModeValue(mode: UseTableViewportOptions["rowHeightMode"]): "fixed" | "auto" {
  if (typeof mode === "string") return mode === "auto" ? "auto" : "fixed"
  if (mode && typeof mode === "object" && "value" in mode) {
    return mode.value === "auto" ? "auto" : "fixed"
  }
  return "fixed"
}

export function resolvePinMode(column: UiTableColumn): ColumnPinMode {
  if (column.sticky === "left" || column.isSystem) {
    return "left"
  }
  if (column.sticky === "right") {
    return "right"
  }

  const raw = column as unknown as Record<string, unknown>
  const pinned = raw?.pinned
  const pin = raw?.pin
  const lock = raw?.lock
  const locked = raw?.locked

  if (pinned === true || pinned === "left" || pin === "left" || lock === "left" || locked === true) {
    return "left"
  }
  if (pinned === "right" || pin === "right" || lock === "right") {
    return "right"
  }
  if (column.stickyLeft) {
    return "left"
  }
  if (column.stickyRight) {
    return "right"
  }
  return "none"
}
