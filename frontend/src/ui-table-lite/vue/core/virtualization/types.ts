import type { UiTableColumn } from "../../../core/types"
import type { ColumnPinMode } from "../../../core/virtualization/types"
import type {
  ColumnMetric as CoreColumnMetric,
  ColumnVirtualizationSnapshot as CoreColumnVirtualizationSnapshot,
} from "../../../core/virtualization/columnSnapshot"
import { createEmptyColumnSnapshot as createCoreEmptyColumnSnapshot } from "../../../core/virtualization/columnSnapshot"

export type { ColumnPinMode }

export type ColumnMetric = CoreColumnMetric<UiTableColumn>

export type ColumnVirtualizationSnapshot = CoreColumnVirtualizationSnapshot<UiTableColumn>

export function createEmptyColumnSnapshot(): ColumnVirtualizationSnapshot {
  return createCoreEmptyColumnSnapshot<UiTableColumn>()
}
