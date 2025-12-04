import { ref } from "vue"
import type { CellEditEvent } from "../../core/types"
import {
  createTableHistory,
  type HistoryEntry,
  type HistoryDirection,
} from "../../core/history/tableHistory"

interface UseTableHistoryOptions {
  applyEntries: (entries: HistoryEntry[], direction: HistoryDirection) => CellEditEvent[]
  onHistoryApplied?: (direction: HistoryDirection, events: CellEditEvent[]) => void
}

export type { HistoryEntry } from "../../core/history/tableHistory"

export function useTableHistory({ applyEntries, onHistoryApplied }: UseTableHistoryOptions) {
  const manager = createTableHistory<CellEditEvent[]>({ applyEntries, onHistoryApplied })

  const undoStack = ref<HistoryEntry[][]>([])
  const redoStack = ref<HistoryEntry[][]>([])
  const isApplyingHistory = ref(false)

  function syncState() {
    const state = manager.getState()
    undoStack.value = state.undoStack
    redoStack.value = state.redoStack
    isApplyingHistory.value = state.isApplying
  }

  syncState()

  function recordHistory(entries: HistoryEntry[]) {
    manager.record(entries)
    syncState()
  }

  function undo() {
    const result = manager.undo()
    syncState()
    return result ?? []
  }

  function redo() {
    const result = manager.redo()
    syncState()
    return result ?? []
  }

  return {
    undo,
    redo,
    recordHistory,
    isApplyingHistory,
    undoStack,
    redoStack,
  }
}
