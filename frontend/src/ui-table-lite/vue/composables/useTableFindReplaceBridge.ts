import { nextTick, onBeforeUnmount, onMounted, watch } from "vue"
import type { ComputedRef, Ref } from "vue"
import type { UiTableColumn, VisibleRow } from "../../core/types"
import type { SelectionPoint } from "./useTableSelection"
import type { useFindReplaceStore } from "@/stores/useFindReplaceStore"

export type FindReplaceStore = ReturnType<typeof useFindReplaceStore>

interface UseTableFindReplaceBridgeOptions {
  findReplace: FindReplaceStore
  processedRows: ComputedRef<VisibleRow[]>
  visibleColumns: ComputedRef<UiTableColumn[]>
  scrollToRow: (rowIndex: number) => void
  scrollToColumn: (columnKey: string) => void
  focusCell: (rowIndex: number, columnKey: string) => void
  getCellValue: (rowIndex: number, columnKey: string) => unknown
  setCellValue: (rowIndex: number, columnKey: string | number, value: unknown) => boolean
  focusContainer: () => void
  undo: () => void
  redo: () => void
  anchorCell: Ref<SelectionPoint | null>
  selectedCell: Ref<SelectionPoint | null>
  isEventInsideTable: (target: EventTarget | null) => boolean
  isEditableTarget: (target: EventTarget | null) => boolean
}

export function useTableFindReplaceBridge(options: UseTableFindReplaceBridgeOptions) {
  const {
    findReplace,
    processedRows,
    visibleColumns,
    scrollToRow,
    scrollToColumn,
    focusCell,
    getCellValue,
    setCellValue,
    focusContainer,
    undo,
    redo,
    anchorCell,
    selectedCell,
    isEventInsideTable,
    isEditableTarget,
  } = options

  function openFindModal() {
    findReplace.activate("find")
  }

  function openReplaceModal() {
    findReplace.activate("replace")
  }

  function closeFindReplace() {
    findReplace.deactivate()
    findReplace.clearResults()
    nextTick(() => focusContainer())
  }

  findReplace.setContext({
    getRows: () => processedRows.value,
    getColumns: () => visibleColumns.value,
    scrollToCell: async (rowIndex: number, columnKey: string) => {
      scrollToRow(rowIndex)
      scrollToColumn(columnKey)
      await nextTick()
    },
    selectCell: (rowIndex: number, columnKey: string) => {
      focusCell(rowIndex, columnKey)
    },
    getCellValue: (rowIndex: number, columnKey: string) => getCellValue(rowIndex, columnKey),
    setCellValue: (rowIndex: number, columnKey: string | number, value: unknown) => setCellValue(rowIndex, columnKey, value),
    focusContainer: () => focusContainer(),
    undo: () => undo(),
    redo: () => redo(),
  })

  watch(
    () => {
      const cell = anchorCell.value ?? selectedCell.value
      return cell ? cell.colIndex : null
    },
    colIndex => {
      if (colIndex == null || colIndex < 0) {
        findReplace.setActiveColumn(null)
        return
      }
      const column = visibleColumns.value[colIndex]
      findReplace.setActiveColumn(column?.key ?? null)
    },
    { immediate: true }
  )

  watch(
    () => visibleColumns.value.map(column => column.key).join("|"),
    () => {
      const cell = anchorCell.value ?? selectedCell.value
      if (!cell) {
        findReplace.setActiveColumn(null)
        return
      }
      const column = visibleColumns.value[cell.colIndex]
      findReplace.setActiveColumn(column?.key ?? null)
    }
  )

  const handleGlobalKeydown = (event: KeyboardEvent) => {
    const key = event.key.toLowerCase()
    const ctrlOrMeta = event.metaKey || event.ctrlKey
    if (!ctrlOrMeta) return

    const insideTable = isEventInsideTable(event.target)
    const active = findReplace.isActive
    const target = event.target as HTMLElement | null
    const pageRoot = document.querySelector("[data-ui-page]")
    const insidePage = pageRoot ? pageRoot.contains(target) : false

    if (!active && !insideTable && !insidePage) return
    if (!active && isEditableTarget(event.target)) return

    if (key === "f") {
      event.preventDefault()
      openFindModal()
    } else if (key === "h") {
      event.preventDefault()
      openReplaceModal()
    }
  }

  onMounted(() => {
    window.addEventListener("keydown", handleGlobalKeydown, true)
  })

  onBeforeUnmount(() => {
    window.removeEventListener("keydown", handleGlobalKeydown, true)
  })

  return {
    closeFindReplace,
  }
}
