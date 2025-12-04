export type TableRowId = string | number

export interface HistoryEntry {
  rowId: TableRowId
  /** @deprecated Provided for backward compatibility with row-index based history. */
  rowIndex: number
  colIndex: number
  columnKey: string
  oldValue: unknown
  newValue: unknown
}

export type HistoryDirection = "undo" | "redo"

export interface TableHistoryState {
  undoStack: HistoryEntry[][]
  redoStack: HistoryEntry[][]
  isApplying: boolean
}

export interface CreateTableHistoryOptions<TResult> {
  applyEntries: (entries: HistoryEntry[], direction: HistoryDirection) => TResult
  onHistoryApplied?: (direction: HistoryDirection, result: TResult) => void
}

export interface TableHistory<TResult> {
  record(entries: HistoryEntry[]): TResult | null
  undo(): TResult | null
  redo(): TResult | null
  getState(): TableHistoryState
}

function cloneEntries(entries: HistoryEntry[]): HistoryEntry[] {
  return entries.map(entry => ({ ...entry }))
}

function cloneStack(stack: HistoryEntry[][]): HistoryEntry[][] {
  return stack.map(batch => batch.map(entry => ({ ...entry })))
}

export function createTableHistory<TResult>(options: CreateTableHistoryOptions<TResult>): TableHistory<TResult> {
  const state: TableHistoryState = {
    undoStack: [],
    redoStack: [],
    isApplying: false,
  }

  function snapshot(): TableHistoryState {
    return {
      undoStack: cloneStack(state.undoStack),
      redoStack: cloneStack(state.redoStack),
      isApplying: state.isApplying,
    }
  }

  function pushRecord(target: "undo" | "redo", entries: HistoryEntry[]): void {
    if (target === "undo") {
      state.undoStack = [...state.undoStack, entries]
    } else {
      state.redoStack = [...state.redoStack, entries]
    }
  }

  function popRecord(target: "undo" | "redo"): HistoryEntry[] | null {
    const source = target === "undo" ? state.undoStack : state.redoStack
    if (!source.length) {
      return null
    }
    const nextStack = source.slice(0, source.length - 1)
    const record = source[source.length - 1]
    if (target === "undo") {
      state.undoStack = nextStack
    } else {
      state.redoStack = nextStack
    }
    return record
  }

  function applyFromStack(source: "undo" | "redo", destination: "undo" | "redo", direction: HistoryDirection): TResult | null {
    const record = popRecord(source)
    if (!record) {
      return null
    }
    state.isApplying = true
    try {
      const ordered = direction === "undo" ? [...record].reverse() : record
      const result = options.applyEntries(ordered, direction)
      pushRecord(destination, record)
      options.onHistoryApplied?.(direction, result)
      return result
    } finally {
      state.isApplying = false
    }
  }

  function record(entries: HistoryEntry[]): TResult | null {
    if (state.isApplying) {
      return null
    }
    const filtered = entries.filter(entry => entry.oldValue !== entry.newValue)
    if (!filtered.length) {
      return null
    }
    const batch = cloneEntries(filtered)
    pushRecord("undo", batch)
    state.redoStack = []
    return null
  }

  function undo(): TResult | null {
    return applyFromStack("undo", "redo", "undo")
  }

  function redo(): TResult | null {
    return applyFromStack("redo", "undo", "redo")
  }

  return {
    record,
    undo,
    redo,
    getState: snapshot,
  }
}
