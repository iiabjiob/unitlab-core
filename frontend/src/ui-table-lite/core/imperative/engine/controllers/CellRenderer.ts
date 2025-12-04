import { mergeClasses, toDisplayValue, applyStyle } from "../../dom.js"
import {
  computeCellClass,
  computeHighlightState,
  resolvePinnedBackground,
  resolvePinnedDividerGradient,
  computeBoxShadow,
  applyCutPreviewStyles,
} from "../../styling.js"
import type { UiTableBodyBindings } from "../../bindings.js"
import type {
  CellSlot,
  CellRenderState,
  ColumnBlueprint,
  ImperativeRendererAdapter,
  ImperativeSelectionSnapshot,
  RowSlot,
  RowSlotRegionState,
} from "../../types.js"
import type { ImperativeRowUpdatePayload } from "../../../viewport/tableViewportController.js"
import type { ResolvedImperativeBodyClassMap } from "../../viewConfig.js"
import type { EditorController } from "./EditorController.js"

declare global {
  interface Window {
    __UNITLAB_DISABLE_FILLERS__?: boolean
  }
}

function shouldUseColumnFillersFlag() {
  if (typeof window !== "undefined" && window.__UNITLAB_DISABLE_FILLERS__) {
    return false
  }
  return true
}

/**
 * Renders individual cells and manages per-cell DOM state.
 */
export class CellRenderer {
  private columnFillersEnabled = shouldUseColumnFillersFlag()

  constructor(
    private readonly body: UiTableBodyBindings,
    private readonly classMap: ResolvedImperativeBodyClassMap,
    private readonly renderer: ImperativeRendererAdapter,
    private readonly editor: EditorController,
  ) {}

  refreshColumnFillersMode(rowSlots: RowSlot[]) {
    const shouldUse = shouldUseColumnFillersFlag()
    if (this.columnFillersEnabled === shouldUse) return
    this.columnFillersEnabled = shouldUse
    if (!shouldUse) {
      this.removeAllColumnFillers(rowSlots)
    }
  }

  renderCell(
    rowSlot: RowSlot,
    regionState: RowSlotRegionState,
    cellIndex: number,
    blueprint: ColumnBlueprint,
    pooled: ImperativeRowUpdatePayload["pool"][number],
    selectionSnapshot: ImperativeSelectionSnapshot | null,
    hasCutPreview: boolean,
  ): number {
    const cellSlot = this.ensureCellSlot(rowSlot, regionState, cellIndex, blueprint)
    const state = this.buildCellState(pooled, blueprint, selectionSnapshot, hasCutPreview)
    const rowData = pooled.entry?.row ?? {}
    return this.applyCell(rowSlot, cellSlot, state, rowData, rowSlot.displayIndex, blueprint)
  }

  teardownCellSlot(cell: CellSlot) {
    if (this.editor.isActive(cell)) {
      this.editor.close()
    }
    if (cell.rowIndexClickHandler) {
      cell.wrapper.removeEventListener("mousedown", cell.rowIndexClickHandler)
      cell.rowIndexClickHandler = undefined
    }
    this.unmountCustomRenderer(cell)
    if (cell.leftFiller?.parentElement) {
      cell.leftFiller.parentElement.removeChild(cell.leftFiller)
    }
    if (cell.rightFiller?.parentElement) {
      cell.rightFiller.parentElement.removeChild(cell.rightFiller)
    }
    if (cell.wrapper.parentElement) {
      cell.wrapper.parentElement.removeChild(cell.wrapper)
    }
    cell.leftFiller = undefined
    cell.rightFiller = undefined
  }

  removeAllColumnFillers(rowSlots: RowSlot[]) {
    for (const rowSlot of rowSlots) {
      for (const cell of rowSlot.cells) {
        if (cell.leftFiller?.parentElement) {
          cell.leftFiller.parentElement.removeChild(cell.leftFiller)
        }
        if (cell.rightFiller?.parentElement) {
          cell.rightFiller.parentElement.removeChild(cell.rightFiller)
        }
        cell.leftFiller = undefined
        cell.rightFiller = undefined
      }
    }
  }

  private ensureCellSlot(
    slot: RowSlot,
    region: RowSlotRegionState,
    index: number,
    blueprint: ColumnBlueprint,
  ): CellSlot {
    const column = blueprint.column
    const expectedType: CellSlot["type"] = column.isSystem
      ? column.key === this.body.SELECTION_COLUMN_KEY
        ? "selection"
        : "index"
      : "data"

    let cell = slot.cells[index]
    if (cell) {
      if (cell.type !== expectedType || !cell.wrapper.isConnected || cell.region !== region.region) {
        this.teardownCellSlot(cell)
        cell = this.createCellSlot(region, blueprint, expectedType)
        slot.cells[index] = cell
      }
    } else {
      cell = this.createCellSlot(region, blueprint, expectedType)
      slot.cells[index] = cell
    }

    this.syncCellFillers(region, cell, blueprint)
    return cell
  }

  private createCellSlot(
    regionState: RowSlotRegionState,
    blueprint: ColumnBlueprint,
    type: CellSlot["type"],
  ): CellSlot {
    const wrapper = document.createElement("div")
    regionState.grid.appendChild(wrapper)
    wrapper.dataset.region = regionState.region

    const column = blueprint.column
    let content: HTMLElement | null = null
    let baseClass: string

    if (type === "selection") {
      baseClass = mergeClasses(this.classMap.selectionCell)
      wrapper.setAttribute("role", "gridcell")
      const checkbox = document.createElement("input")
      checkbox.type = "checkbox"
      checkbox.setAttribute("aria-label", "Select row")
      checkbox.className = this.classMap.selectionCheckbox
      wrapper.appendChild(checkbox)
      content = checkbox
    } else if (type === "index") {
      baseClass = mergeClasses(this.classMap.indexCell)
      wrapper.setAttribute("role", "rowheader")
      const span = document.createElement("span")
      span.className = this.classMap.indexText
      wrapper.appendChild(span)
      content = span
    } else {
      baseClass = this.resolveBlueprintBaseClass(blueprint)
      wrapper.setAttribute("role", "gridcell")
      const span = document.createElement("span")
      span.className = this.classMap.dataCellText
      span.dataset.autoResizeTarget = ""
      wrapper.appendChild(span)
      content = span
    }

    wrapper.className = baseClass
    wrapper.tabIndex = -1

    const cell: CellSlot = {
      wrapper,
      content,
      columnKey: column.key,
      columnIndex: blueprint.binding.columnIndex,
      type,
      region: regionState.region,
      value: null,
      classCache: "",
      styleCache: new Map(),
      baseClass,
      leftFiller: undefined,
      rightFiller: undefined,
      rowIndexClickHandler: undefined,
      rendererHost: undefined,
    }

    if (type === "selection") {
      cell.checkbox = content as HTMLInputElement
    }

    if (type === "index") {
      const handler = (event: MouseEvent) => {
        event.stopPropagation()
        const rawIndex = Number.parseInt(cell.wrapper.dataset.rowIndex ?? "-1", 10)
        if (!Number.isFinite(rawIndex) || rawIndex < 0) return
        this.body.onRowIndexClick(rawIndex, event)
      }
      wrapper.addEventListener("mousedown", handler)
      cell.rowIndexClickHandler = handler
    }

    return cell
  }

  private resolveBlueprintBaseClass(blueprint: ColumnBlueprint): string {
    if (blueprint.cachedClassName) {
      return blueprint.cachedClassName
    }
    const column = blueprint.column
    const bindingClass = blueprint.binding?.cellClass ?? ""
    const alignClass = column.align === "right"
      ? this.classMap.align.right
      : column.align === "center"
        ? this.classMap.align.center
        : this.classMap.align.left
    const base = mergeClasses(
      this.classMap.dataCell,
      this.body.bodyCellClass(column),
      bindingClass,
      alignClass,
    )
    blueprint.cachedClassName = base
    return base
  }

  private syncCellFillers(region: RowSlotRegionState, cell: CellSlot, blueprint: ColumnBlueprint) {
    if (!this.columnFillersEnabled) {
      if (cell.leftFiller) {
        const filler = cell.leftFiller
        if (filler.parentElement) filler.parentElement.removeChild(filler)
      }
      if (cell.rightFiller) {
        const filler = cell.rightFiller
        if (filler.parentElement) filler.parentElement.removeChild(filler)
      }
      cell.leftFiller = undefined
      cell.rightFiller = undefined
      return
    }

    const rawHeight = this.body.rowHeightDom.value
    const heightValue =
      typeof rawHeight === "number" && Number.isFinite(rawHeight) && rawHeight > 0 ? `${rawHeight}px` : ""

    if (blueprint.renderable.showLeftFiller) {
      if (!cell.leftFiller || !cell.leftFiller.isConnected) {
        const filler = this.createFillerElement("left")
        region.grid.insertBefore(filler, cell.wrapper)
        cell.leftFiller = filler
      }
      if (cell.leftFiller) {
        if (heightValue) {
          cell.leftFiller.style.height = heightValue
        } else {
          cell.leftFiller.style.removeProperty("height")
        }
      }
    } else if (cell.leftFiller) {
      const filler = cell.leftFiller
      if (filler.parentElement) {
        filler.parentElement.removeChild(filler)
      }
      cell.leftFiller = undefined
    }

    if (blueprint.renderable.showRightFiller) {
      if (!cell.rightFiller || !cell.rightFiller.isConnected) {
        const filler = this.createFillerElement("right")
        const reference = cell.wrapper.nextSibling
        if (reference) {
          region.grid.insertBefore(filler, reference)
        } else {
          region.grid.appendChild(filler)
        }
        cell.rightFiller = filler
      }
      if (cell.rightFiller) {
        if (heightValue) {
          cell.rightFiller.style.height = heightValue
        } else {
          cell.rightFiller.style.removeProperty("height")
        }
      }
    } else if (cell.rightFiller) {
      const filler = cell.rightFiller
      if (filler.parentElement) {
        filler.parentElement.removeChild(filler)
      }
      cell.rightFiller = undefined
    }
  }

  private createFillerElement(side: "left" | "right"): HTMLDivElement {
    const filler = document.createElement("div")
    filler.className = `ui-table__column-filler ui-table__column-filler--${side}`
    filler.setAttribute("aria-hidden", "true")
    return filler
  }

  private buildCellState(
    pooled: ImperativeRowUpdatePayload["pool"][number],
    blueprint: ColumnBlueprint,
    selectionSnapshot: ImperativeSelectionSnapshot | null,
    hasCutPreview: boolean,
  ): CellRenderState {
    const column = blueprint.column
    const binding = blueprint.binding
    const rowIndex = pooled.displayIndex
    const columnIndex = binding.columnIndex
    const originalIndex = pooled.entry?.originalIndex ?? undefined
    const zoom = this.body.zoom.value
    const cellKey = `${rowIndex}:${columnIndex}`
    const cellSnapshot = selectionSnapshot?.cells.get(cellKey)
    const isRowSelected = selectionSnapshot
      ? selectionSnapshot.rowSelection.has(rowIndex)
      : this.body.isRowFullySelected(rowIndex)
    const isColumnSelected = selectionSnapshot
      ? selectionSnapshot.columnSelection.has(columnIndex)
      : this.body.isColumnFullySelected(columnIndex)
    const isCursor = selectionSnapshot
      ? selectionSnapshot.cursorKey === cellKey
      : this.body.isSelectionCursorCell(rowIndex, columnIndex)
    const isAnchor = selectionSnapshot
      ? selectionSnapshot.anchorKey === cellKey
      : this.body.isSelectionAnchorCell(rowIndex, columnIndex)
    const inSelection = cellSnapshot
      ? cellSnapshot.isSelected
      : this.body.isCellSelected(rowIndex, columnIndex)
    const inRange = cellSnapshot
      ? cellSnapshot.isRangeSelected
      : this.body.isCellInSelectionRange(rowIndex, columnIndex)
    const inFillPreview = cellSnapshot
      ? cellSnapshot.isFillPreview
      : this.body.isCellInFillPreview(rowIndex, columnIndex)
    const fillPreviewEdges = cellSnapshot
      ? cellSnapshot.fillPreviewEdges
      : this.body.getFillPreviewEdges(rowIndex, columnIndex)
    const rangeEdges = cellSnapshot
      ? cellSnapshot.rangeEdges
      : this.body.getSelectionEdges(rowIndex, columnIndex)

    let isCutPreview = false
    let cutPreviewEdges = null
    if (hasCutPreview) {
      if (cellSnapshot) {
        isCutPreview = cellSnapshot.isCutPreview
        cutPreviewEdges = cellSnapshot.cutPreviewEdges
      } else {
        isCutPreview = this.body.isCellInCutPreview(rowIndex, columnIndex)
        cutPreviewEdges = this.body.getCutPreviewEdges(rowIndex, columnIndex)
      }
    }

    const snapshotRowClass = selectionSnapshot?.rowClasses.get(rowIndex)
    const rowHeaderClass = snapshotRowClass?.value ?? (rowIndex >= 0 ? mergeClasses(this.body.rowHeaderClass(rowIndex)) : "")

    return {
      row: pooled.entry?.row,
      column,
      rowIndex,
      columnIndex,
      originalIndex,
      zoom,
      isSelected: inSelection,
      isAnchor,
      isCursor,
      isRowSelected,
      isColumnSelected,
      isRangeSelected: inRange,
      isFillPreview: inFillPreview,
      fillPreviewEdges,
      isCutPreview,
      cutPreviewEdges,
      rangeEdges,
      editable: this.body.isColumnEditable(column),
      validationError: this.body.getValidationError(rowIndex, columnIndex),
      tabIndex: this.body.getCellTabIndex(rowIndex, columnIndex),
      ariaRowIndex: this.body.getAriaRowIndex(rowIndex),
      ariaColIndex: binding.ariaColIndex,
      sticky: binding.sticky,
      stickySide: binding.stickySide,
      stickyLeftOffset: binding.stickyLeftOffset,
      stickyRightOffset: binding.stickyRightOffset,
      stickyTopOffset: this.body.getStickyTopOffset(pooled.entry),
      stickyTop: Boolean(pooled.entry?.stickyTop ?? pooled.entry?.row?.stickyTop),
      searchMatch: this.body.isSearchMatchCell(pooled.entry?.displayIndex ?? rowIndex, column.key),
      activeSearchMatch: this.body.isActiveSearchMatchCell(pooled.entry?.displayIndex ?? rowIndex, column.key),
      rowHeaderClass,
    }
  }

  private buildRendererSlotProps(rowData: any, rowSlot: RowSlot, state: CellRenderState, blueprint: ColumnBlueprint) {
    return {
      value: rowData?.[blueprint.column.key],
      row: rowData,
      column: blueprint.column,
      rowIndex: state.rowIndex,
      colIndex: state.columnIndex,
      originalRowIndex: rowSlot.originalIndex ?? undefined,
      displayRowIndex: rowSlot.displayIndex,
    }
  }

  private renderCustomContent(
    cell: CellSlot,
    rowSlot: RowSlot,
    state: CellRenderState,
    rowData: any,
    blueprint: ColumnBlueprint,
  ) {
    const slotName = `cell-${cell.columnKey}`
    if (!this.body.tableSlots?.[slotName]) {
      this.unmountCustomRenderer(cell)
      return
    }

    if (!cell.rendererHost) {
      const host = document.createElement("div")
      host.className = this.classMap.customRendererHost
      cell.wrapper.appendChild(host)
      cell.rendererHost = host
    }

    if (cell.content) {
      cell.content.style.display = "none"
    }

    const slotProps = this.buildRendererSlotProps(rowData, rowSlot, state, blueprint)
    this.renderer.mountCustomCell({ slotName, columnKey: cell.columnKey, props: slotProps, host: cell.rendererHost })
  }

  private unmountCustomRenderer(cell: CellSlot) {
    if (cell.rendererHost) {
      this.renderer.unmountCustomCell(cell.rendererHost)
      if (cell.rendererHost.parentElement) {
        cell.rendererHost.parentElement.removeChild(cell.rendererHost)
      }
      cell.rendererHost = undefined
    }
    if (cell.content) {
      cell.content.style.display = ""
    }
  }

  private applyCell(
    rowSlot: RowSlot,
    cell: CellSlot,
    state: CellRenderState,
    rowData: any,
    displayIndex: number,
    blueprint: ColumnBlueprint,
  ): number {
    let mutations = 0
    const column = blueprint.column
    let editing = this.editor.isActive(cell)

    if (cell.columnKey !== column.key) {
      cell.columnKey = column.key
    }
    if (cell.columnIndex !== blueprint.binding.columnIndex) {
      cell.columnIndex = blueprint.binding.columnIndex
    }

    let value: unknown
    let displayValue = ""

    if (cell.type === "index") {
      value = displayIndex + 1
      displayValue = String(displayIndex + 1)
    } else if (cell.type === "selection") {
      value = this.body.isCheckboxRowSelected(rowData)
    } else {
      value = rowData?.[column.key]
      displayValue = toDisplayValue(value)
    }

    if (cell.value !== value) {
      cell.value = value
      mutations += 1
    }

    const hasCustomRenderer = cell.type === "data" && this.body.hasCustomRenderer(column.key)
    if (hasCustomRenderer) {
      this.renderCustomContent(cell, rowSlot, state, rowData, blueprint)
    } else {
      this.unmountCustomRenderer(cell)
      if (cell.type !== "selection" && cell.content) {
        cell.content.textContent = displayValue
        cell.content.style.display = editing ? "none" : ""
      }
    }

    if (cell.rendererHost) {
      cell.rendererHost.style.display = editing ? "none" : ""
    }

    if (editing && !state.isSelected) {
      this.editor.commitSilently()
      editing = false
    }
    const highlightActive = computeHighlightState({ cell, state, column, editing })
    if (highlightActive) {
      cell.wrapper.dataset.highlighted = "true"
    } else {
      delete cell.wrapper.dataset.highlighted
    }

    const className = computeCellClass({
      cell,
      state,
      column,
      editing,
      body: this.body,
      classMap: this.classMap,
    })
    if (cell.classCache !== className) {
      cell.classCache = className
      cell.wrapper.className = className
    }

    cell.wrapper.dataset.rowIndex = String(displayIndex)
    cell.wrapper.dataset.colKey = cell.columnKey
    cell.wrapper.dataset.colIndex = String(cell.columnIndex)
    if (cell.type === "index") {
      cell.wrapper.tabIndex = -1
    } else {
      cell.wrapper.tabIndex = state.tabIndex ?? -1
    }
    cell.wrapper.setAttribute("role", cell.type === "index" ? "rowheader" : "gridcell")
    cell.wrapper.setAttribute("aria-rowindex", String(state.ariaRowIndex ?? displayIndex + 1))
    cell.wrapper.setAttribute("aria-colindex", String(state.ariaColIndex ?? cell.columnIndex + 1))

    const style: Record<string, unknown> = {}
    if (state.stickySide === "left") {
      style.position = "sticky"
      style.left = `${state.stickyLeftOffset ?? 0}px`
      style.right = ""
    } else if (state.stickySide === "right") {
      style.position = "sticky"
      style.right = `${state.stickyRightOffset ?? 0}px`
      style.left = ""
    } else {
      style.position = ""
      style.left = ""
      style.right = ""
    }
    if (state.stickyTop) {
      style.top = `${state.stickyTopOffset ?? 0}px`
    } else {
      style.top = ""
    }
    if (column.isSystem) {
      const systemStyle = this.body.systemColumnStyle(column) as Record<string, unknown>
      Object.assign(style, systemStyle)
    }

    const pinnedBackground = resolvePinnedBackground({ state, editing })
    const pinnedDivider = resolvePinnedDividerGradient(state)

    style.backgroundColor = pinnedBackground
    style["background-image"] = pinnedDivider
    style["background-repeat"] = pinnedDivider ? "no-repeat" : ""
    style["background-size"] = pinnedDivider ? "100% 100%" : ""
    style["box-shadow"] = computeBoxShadow({ cell, state, column, editing })
    applyCutPreviewStyles({ state, style })

    applyStyle(cell.wrapper, cell.styleCache, style)

    if (cell.type === "selection" && cell.checkbox) {
      const checked = Boolean(value)
      if (cell.checkbox.checked !== checked) {
        cell.checkbox.checked = checked
      }
      cell.checkbox.name = this.body.rowSelectionName.value
      cell.checkbox.onchange = event => {
        event.stopPropagation()
        this.body.handleRowCheckboxToggle(rowData)
      }
    }

    return mutations
  }
}
