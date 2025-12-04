import { BASE_ROW_HEIGHT } from "../utils/constants"
import type { UiTableColumn, VisibleRow } from "../types"
import type { ColumnMetric } from "../virtualization/columnSnapshot"
import type { WritableSignal } from "../runtime/signals"

export interface RowPoolItem {
  poolIndex: number
  entry: VisibleRow | null
  displayIndex: number
  rowIndex: number
}

export interface TableViewportState {
  startIndex: number
  endIndex: number
  visibleCount: number
  poolSize: number
  totalRowCount: number
  overscanLeading: number
  overscanTrailing: number
}

export interface TableViewportInputSignals {
  scrollTop: WritableSignal<number>
  scrollLeft: WritableSignal<number>
  viewportHeight: WritableSignal<number>
  viewportWidth: WritableSignal<number>
  virtualizationEnabled: WritableSignal<boolean>
}

export interface TableViewportCoreSignals {
  totalRowCount: WritableSignal<number>
  effectiveRowHeight: WritableSignal<number>
  visibleCount: WritableSignal<number>
  poolSize: WritableSignal<number>
  totalContentHeight: WritableSignal<number>
  startIndex: WritableSignal<number>
  endIndex: WritableSignal<number>
  overscanLeading: WritableSignal<number>
  overscanTrailing: WritableSignal<number>
}

export interface TableViewportDerivedRowSignals {
  visibleRange: WritableSignal<{ start: number; end: number }>
}

export interface TableViewportDerivedColumnSignals {
  visibleColumns: WritableSignal<UiTableColumn[]>
  visibleColumnEntries: WritableSignal<ColumnMetric<UiTableColumn>[]>
  visibleScrollableColumns: WritableSignal<UiTableColumn[]>
  visibleScrollableEntries: WritableSignal<ColumnMetric<UiTableColumn>[]>
  pinnedLeftColumns: WritableSignal<UiTableColumn[]>
  pinnedLeftEntries: WritableSignal<ColumnMetric<UiTableColumn>[]>
  pinnedRightColumns: WritableSignal<UiTableColumn[]>
  pinnedRightEntries: WritableSignal<ColumnMetric<UiTableColumn>[]>
  leftPadding: WritableSignal<number>
  rightPadding: WritableSignal<number>
  columnWidthMap: WritableSignal<Map<string, number>>
  visibleStartCol: WritableSignal<number>
  visibleEndCol: WritableSignal<number>
  scrollableRange: WritableSignal<{ start: number; end: number }>
  columnVirtualState: WritableSignal<{
    start: number
    end: number
    visibleStart: number
    visibleEnd: number
    overscanLeading: number
    overscanTrailing: number
    poolSize: number
    visibleCount: number
    totalCount: number
    indexColumnWidth: number
    pinnedRightWidth: number
  }>
}

export interface TableViewportMetricSignals {
  debugMode: WritableSignal<boolean>
  fps: WritableSignal<number>
  frameTime: WritableSignal<number>
  droppedFrames: WritableSignal<number>
  layoutReads: WritableSignal<number>
  layoutWrites: WritableSignal<number>
  syncScrollRate: WritableSignal<number>
  heavyUpdateRate: WritableSignal<number>
  virtualizerUpdates: WritableSignal<number>
  virtualizerSkips: WritableSignal<number>
}

export interface TableViewportDerivedSignals {
  rows: TableViewportDerivedRowSignals
  columns: TableViewportDerivedColumnSignals
  metrics: TableViewportMetricSignals
}

export interface TableViewportSignals {
  input: TableViewportInputSignals
  core: TableViewportCoreSignals
  derived: TableViewportDerivedSignals
  dispose(): void
}

export type SignalFactory = <T>(initial: T) => WritableSignal<T>

function cloneInitial<T>(source: T): T {
  if (Array.isArray(source)) {
    return [] as unknown as T
  }
  if (source instanceof Map) {
    return new Map() as unknown as T
  }
  if (source && typeof source === "object") {
    return { ...(source as Record<string, unknown>) } as T
  }
  return source
}

export function createTableViewportSignals(createSignal: SignalFactory): TableViewportSignals {
  const resets: Array<() => void> = []

  const track = <T,>(initial: T, factory?: () => T): WritableSignal<T> => {
    const signal = createSignal(initial)
    const resetFactory = factory ?? (() => cloneInitial(initial))
    resets.push(() => {
      signal.value = resetFactory()
    })
    return signal
  }

  const columnVirtualStateFactory = () => ({
    start: 0,
    end: 0,
    visibleStart: 0,
    visibleEnd: 0,
    overscanLeading: 0,
    overscanTrailing: 0,
    poolSize: 0,
    visibleCount: 0,
    totalCount: 0,
    indexColumnWidth: 0,
    pinnedRightWidth: 0,
  })

  const input: TableViewportInputSignals = {
    scrollTop: track(0),
    scrollLeft: track(0),
    viewportHeight: track(0),
    viewportWidth: track(0),
    virtualizationEnabled: track(true),
  }

  const core: TableViewportCoreSignals = {
    totalRowCount: track(0),
    effectiveRowHeight: track(BASE_ROW_HEIGHT),
    visibleCount: track(0),
    poolSize: track(0),
    totalContentHeight: track(0),
    startIndex: track(0),
    endIndex: track(0),
    overscanLeading: track(0),
    overscanTrailing: track(0),
  }

  const derivedRows: TableViewportDerivedRowSignals = {
    visibleRange: track({ start: 0, end: 0 }, () => ({ start: 0, end: 0 })),
  }

  const derivedColumns: TableViewportDerivedColumnSignals = {
    visibleColumns: track<UiTableColumn[]>([], () => []),
    visibleColumnEntries: track<ColumnMetric<UiTableColumn>[]>([], () => []),
    visibleScrollableColumns: track<UiTableColumn[]>([], () => []),
    visibleScrollableEntries: track<ColumnMetric<UiTableColumn>[]>([], () => []),
    pinnedLeftColumns: track<UiTableColumn[]>([], () => []),
    pinnedLeftEntries: track<ColumnMetric<UiTableColumn>[]>([], () => []),
    pinnedRightColumns: track<UiTableColumn[]>([], () => []),
    pinnedRightEntries: track<ColumnMetric<UiTableColumn>[]>([], () => []),
    leftPadding: track(0),
    rightPadding: track(0),
    columnWidthMap: track(new Map<string, number>(), () => new Map<string, number>()),
    visibleStartCol: track(0),
    visibleEndCol: track(0),
    scrollableRange: track({ start: 0, end: 0 }, () => ({ start: 0, end: 0 })),
    columnVirtualState: track(columnVirtualStateFactory(), columnVirtualStateFactory),
  }

  const derivedMetrics: TableViewportMetricSignals = {
    debugMode: track(false),
    fps: track(0),
    frameTime: track(0),
    droppedFrames: track(0),
    layoutReads: track(0),
    layoutWrites: track(0),
    syncScrollRate: track(0),
    heavyUpdateRate: track(0),
    virtualizerUpdates: track(0),
    virtualizerSkips: track(0),
  }

  const derived: TableViewportDerivedSignals = {
    rows: derivedRows,
    columns: derivedColumns,
    metrics: derivedMetrics,
  }

  return {
    input,
    core,
    derived,
    dispose: () => {
      for (const reset of resets) {
        reset()
      }
    },
  }
}
