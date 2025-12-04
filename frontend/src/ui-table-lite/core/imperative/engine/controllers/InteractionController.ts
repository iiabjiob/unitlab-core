import { FILL_HANDLE_SIZE } from "../../../utils/constants.js"
import { mergeClasses } from "../../dom.js"
import {
  createMeasurementQueue,
  type MeasurementQueue,
} from "../../../runtime/measurementQueue.js"
import type { ImperativeContainerMap, UiTableBodyBindings } from "../../bindings.js"
import type {
  CellSlot,
  ColumnBlueprint,
  InteractionMode,
  MoveSelectionGeometry,
  MoveSelectionState,
  RowSlot,
  ImperativeBodyRegion,
} from "../../types.js"
import { InteractionMode as InteractionModeEnum } from "../../types.js"
import type { BlueprintManager } from "./BlueprintManager.js"
import type { RowController } from "./RowController.js"
import type { SelectionAreaDescriptor } from "../../bindings.js"
import type { ResolvedImperativeBodyClassMap } from "../../viewConfig.js"

/**
 * Handles pointer/keyboard interactions, selection moves, and editing triggers.
 */
const REGIONS: ImperativeBodyRegion[] = ["pinned-left", "main", "pinned-right"]

type CellRect = ReturnType<RowController["getCellRectWithinContainer"]>

export class InteractionController {
  private containers = new Map<ImperativeBodyRegion, HTMLDivElement>()
  private attachedContainers = new Set<HTMLDivElement>()
  private primaryContainer: HTMLDivElement | null = null
  private interactionMode: InteractionMode = InteractionModeEnum.None
  private moveOverlay: HTMLDivElement | null = null
  private moveState: MoveSelectionState | null = null
  private moveCursorActive = false
  private moveListenersAttached = false
  private readonly moveGeometryCache: MoveSelectionGeometry = {
    containerLeft: 0,
    containerTop: 0,
    width: 0,
    height: 0,
    rowHeight: 0,
    avgColumnWidth: 0,
  }
  private totalRowCount = 0
  private rowHeight = 0
  private totalColumnCount = 0
  private readonly measurementQueue: MeasurementQueue
  private readonly ownsMeasurementQueue: boolean

  constructor(
    private readonly mode: { readonly value: boolean },
    private readonly body: UiTableBodyBindings,
    private readonly rowController: RowController,
    private readonly blueprintManager: BlueprintManager,
    private readonly classMap: ResolvedImperativeBodyClassMap,
    private readonly beginCellEdit: (rowSlot: RowSlot, cellSlot: CellSlot, blueprint: ColumnBlueprint) => void,
    measurementQueue?: MeasurementQueue,
  ) {
    this.measurementQueue = measurementQueue ?? createMeasurementQueue()
    this.ownsMeasurementQueue = !measurementQueue
  }

  setContainers(map: ImperativeContainerMap | null, primaryRegion: ImperativeBodyRegion) {
    const nextEntries: Array<[ImperativeBodyRegion, HTMLDivElement]> = []
    if (map) {
      for (const region of REGIONS) {
        const element = map[region] ?? null
        if (element) {
          nextEntries.push([region, element])
        }
      }
    }

    const nextSet = new Set<HTMLDivElement>(nextEntries.map(([, element]) => element))

    for (const existing of this.attachedContainers) {
      if (!nextSet.has(existing)) {
        this.removeListeners(existing)
      }
    }

    for (const [, element] of nextEntries) {
      if (!this.attachedContainers.has(element)) {
        this.addListeners(element)
      }
    }

  this.attachedContainers = nextSet
  this.containers = new Map(nextEntries)
  const previousPrimary = this.primaryContainer
  this.primaryContainer = this.containers.get(primaryRegion) ?? (nextEntries.length ? nextEntries[0][1] : null)

    if (previousPrimary !== this.primaryContainer) {
      this.relocateOverlayToPrimary()
    } else {
      this.reappendOverlay()
    }

    if (!this.primaryContainer) {
      this.cancelMoveSelection()
      this.clearOverlay()
    }

    this.updateCursorStyles()
  }

  detach() {
    if (!this.attachedContainers.size && !this.moveOverlay) {
      this.primaryContainer = null
      this.moveCursorActive = false
      return
    }

    for (const container of this.attachedContainers) {
      this.removeListeners(container)
    }
    this.attachedContainers.clear()
  this.containers.clear()
  this.primaryContainer = null
    this.cancelMoveSelection()
    this.clearOverlay()
    this.moveOverlay = null
    this.moveCursorActive = false
  }

  private addListeners(container: HTMLDivElement) {
    container.addEventListener("mousedown", this.handlePointerDown)
    container.addEventListener("dblclick", this.handleDoubleClick)
    container.addEventListener("mousemove", this.handlePointerMove)
    container.addEventListener("mouseleave", this.handlePointerLeave)
    container.addEventListener("focusin", this.handleFocusIn)
  }

  private removeListeners(container: HTMLDivElement) {
    container.removeEventListener("mousedown", this.handlePointerDown)
    container.removeEventListener("dblclick", this.handleDoubleClick)
    container.removeEventListener("mousemove", this.handlePointerMove)
    container.removeEventListener("mouseleave", this.handlePointerLeave)
    container.removeEventListener("focusin", this.handleFocusIn)
  }

  private getPrimaryContainer(): HTMLDivElement | null {
    return this.primaryContainer
  }

  private relocateOverlayToPrimary() {
    const overlay = this.moveOverlay
    if (!overlay) return
    if (overlay.parentElement) {
      overlay.parentElement.removeChild(overlay)
    }
    const host = this.getPrimaryContainer()
    if (host) {
      host.appendChild(overlay)
    }
  }

  private clearOverlay() {
    if (this.moveOverlay?.parentElement) {
      this.moveOverlay.parentElement.removeChild(this.moveOverlay)
    }
  }

  private forEachContainer(callback: (container: HTMLDivElement) => void) {
    for (const container of this.attachedContainers) {
      callback(container)
    }
  }

  private updateCursorStyles() {
    let cursor = "default"
    if (this.interactionMode === InteractionModeEnum.Moving) {
      cursor = "grabbing"
    } else if (this.moveCursorActive) {
      cursor = "grab"
    }
    this.forEachContainer(container => {
      container.style.cursor = cursor
    })
  }

  private getRegionForCell(element: HTMLElement): ImperativeBodyRegion {
    const region = element.dataset?.region as ImperativeBodyRegion | undefined
    if (region === "pinned-left" || region === "pinned-right" || region === "main") {
      return region
    }
    return this.rowController.getPrimaryRegion()
  }

  private measureElementRect(element: HTMLElement): DOMRect {
    let rect: DOMRect | null = null
    const handle = this.measurementQueue.schedule(() => {
      rect = element.getBoundingClientRect()
      return rect
    })
    this.measurementQueue.flush()
    void handle.promise.catch(() => {})
    if (!rect) {
      rect = element.getBoundingClientRect()
    }
    return rect
  }

  private getCellRectRelativeToPrimary(cell: HTMLElement): CellRect {
    const rect = this.rowController.getCellRectWithinContainer(cell)
    const host = this.getPrimaryContainer()
    if (!host) {
      return rect
    }
    const region = this.getRegionForCell(cell)
    const container = this.rowController.getContainerForRegion(region)
    if (container && container !== host) {
      const hostRect = this.measureElementRect(host)
      const containerRect = this.measureElementRect(container)
      const deltaLeft = containerRect.left - hostRect.left
      const deltaTop = containerRect.top - hostRect.top
      rect.left += deltaLeft
      rect.right += deltaLeft
      rect.top += deltaTop
      rect.bottom += deltaTop
    }
    return rect
  }

  dispose() {
    this.detach()
    if (this.ownsMeasurementQueue) {
      this.measurementQueue.dispose()
    }
  }

  setMetrics(metrics: { totalRowCount: number; rowHeight: number; totalColumnCount: number }) {
    this.totalRowCount = metrics.totalRowCount
    this.rowHeight = metrics.rowHeight
    this.totalColumnCount = metrics.totalColumnCount
  }

  reappendOverlay() {
    const host = this.getPrimaryContainer()
    if (this.moveOverlay && host && !this.moveOverlay.isConnected) {
      host.appendChild(this.moveOverlay)
    }
  }

  private handlePointerDown = (event: MouseEvent) => {
    if (!this.mode.value) return
    const target = event.target as HTMLElement | null
    if (!target) return
    this.updateMoveCursor(event, this.rowController.findAncestorCell(target))

    const interactive = target.closest("input,select,textarea,[contenteditable='true'],button")
    if (interactive) return

    const cell = this.rowController.findAncestorCell(target)
    if (!cell) return

    const rowIndex = Number.parseInt(cell.dataset.rowIndex ?? "-1", 10)
    const colKey = cell.dataset.colKey ?? ""
    const colIndex = Number.parseInt(cell.dataset.colIndex ?? "-1", 10)
    if (rowIndex < 0 || colIndex < 0) return

    if (this.tryStartFillDrag(cell, rowIndex, colIndex, event)) {
      return
    }

    if (this.tryStartSelectionMove(cell, rowIndex, colIndex, event)) {
      return
    }

    this.body.onCellSelect({ rowIndex, key: colKey, colIndex, focus: true, event })
    if (event.button === 0 && !(event.shiftKey || event.metaKey || event.ctrlKey || event.altKey)) {
      this.body.onCellDragStart({ rowIndex, colIndex, event })
    }
  }

  private handleDoubleClick = (event: MouseEvent) => {
    if (!this.mode.value) return
    const target = event.target as HTMLElement
    const cellElement = this.rowController.findAncestorCell(target)
    if (!cellElement) return

    const rowIndex = Number.parseInt(cellElement.dataset.rowIndex ?? "-1", 10)
    const colKey = cellElement.dataset.colKey ?? ""
    const colIndex = Number.parseInt(cellElement.dataset.colIndex ?? "-1", 10)
    if (rowIndex < 0 || colIndex < 0) return

    if (this.tryAutoFillDown(cellElement, rowIndex, colIndex, event)) {
      return
    }

    const rowSlot = this.rowController.findRowSlotByDisplayIndex(rowIndex)
    if (!rowSlot) return
    const cellSlot = this.rowController.findCellSlot(rowSlot, colKey)
    if (!cellSlot || cellSlot.type !== "data") return

    const blueprint = this.blueprintManager.getBlueprintByKey(colKey)
    if (!blueprint) return
    this.beginCellEdit(rowSlot, cellSlot, blueprint)
  }

  private handlePointerMove = (event: MouseEvent) => {
    if (!this.mode.value) return
    if (this.interactionMode === InteractionModeEnum.Moving) return
    this.updateMoveCursor(event)
    if ((event.buttons & 1) !== 0) {
      this.setMoveCursorActive(false)
    }
    if (!(event.buttons & 1)) return
    if (this.body.isFillDragging()) return
    const cell = this.rowController.findAncestorCell(event.target as HTMLElement)
    if (!cell) return
    const rowIndex = Number.parseInt(cell.dataset.rowIndex ?? "-1", 10)
    const colIndex = Number.parseInt(cell.dataset.colIndex ?? "-1", 10)
    if (rowIndex < 0 || colIndex < 0) return
    this.body.onCellDragEnter({ rowIndex, colIndex, event })
  }

  private handlePointerLeave = () => {
    if (this.interactionMode === InteractionModeEnum.Moving) return
    this.setMoveCursorActive(false)
  }

  private handleFocusIn = (event: FocusEvent) => {
    if (!this.mode.value) return
    const cell = this.rowController.findAncestorCell(event.target as HTMLElement)
    if (!cell) return
    const rowIndex = Number.parseInt(cell.dataset.rowIndex ?? "-1", 10)
    const colIndex = Number.parseInt(cell.dataset.colIndex ?? "-1", 10)
    const colKey = cell.dataset.colKey ?? ""
    if (rowIndex < 0 || colIndex < 0) return
    this.body.onCellComponentFocus({ rowIndex, colIndex, columnKey: colKey })
  }

  private tryStartFillDrag(cell: HTMLElement, rowIndex: number, colIndex: number, event: MouseEvent) {
    if (event.button !== 0) return false
    if (!this.body.startFillDrag) return false
    if (this.body.isFillDragging()) return false
    if (!this.isFillHandleZone(cell, rowIndex, colIndex, event)) return false
    this.body.startFillDrag(event)
    return true
  }

  private tryStartSelectionMove(cell: HTMLElement, rowIndex: number, colIndex: number, event: MouseEvent) {
    if (event.button !== 0) return false
    if (!this.isMoveInteractionZone(cell, rowIndex, colIndex, event)) return false
    return this.startMoveSelection(rowIndex, colIndex, event)
  }

  private tryAutoFillDown(cell: HTMLElement, rowIndex: number, colIndex: number, event: MouseEvent) {
    if (!this.body.autoFillDown) return false
    if (!this.isFillHandleZone(cell, rowIndex, colIndex, event)) return false
    event.preventDefault()
    event.stopPropagation()
    this.body.autoFillDown(event)
    return true
  }

  private isFillHandleZone(cell: HTMLElement, rowIndex: number, colIndex: number, event: MouseEvent) {
    if (!this.body.isCellSelected(rowIndex, colIndex)) return false
    if (this.body.isRowFullySelected(rowIndex)) return false
    if (this.body.isColumnFullySelected(colIndex)) return false
    const edges = this.body.getSelectionEdges(rowIndex, colIndex)
    if (!edges || !edges.active || !edges.bottom || !edges.right) return false
    const colKey = cell.dataset.colKey ?? ""
    const blueprint = this.blueprintManager.getBlueprintByKey(colKey)
    if (!blueprint || blueprint.column.isSystem) return false
    const rect = this.measureCellRect(cell)
    const handleSize = Math.min(FILL_HANDLE_SIZE + 2, rect.width, rect.height)
    if (event.clientX < rect.right - handleSize) return false
    if (event.clientY < rect.bottom - handleSize) return false
    return true
  }

  private isMoveInteractionZone(cell: HTMLElement, rowIndex: number, colIndex: number, event: MouseEvent | null) {
    if (!this.body.canMoveActiveSelection()) return false
    if (this.body.isFillDragging()) return false
    if (!this.body.isCellInSelectionRange(rowIndex, colIndex)) return false
    const edges = this.body.getSelectionEdges(rowIndex, colIndex)
    if (!edges) return false
    if (event && this.isFillHandleZone(cell, rowIndex, colIndex, event)) return false
  const rect = this.measureCellRect(cell)
    const baseThreshold = Math.min(rect.height, rect.width)
    const threshold = Math.min(8, baseThreshold / 2)
    if (!event) return true
    const withinX = event.clientX >= rect.left && event.clientX <= rect.right
    const withinY = event.clientY >= rect.top && event.clientY <= rect.bottom
    if (!withinX && !withinY) return false
    const nearTop = edges.top && withinX && event.clientY <= rect.top + threshold
    const nearBottom = edges.bottom && withinX && event.clientY >= rect.bottom - threshold
    const nearLeft = edges.left && withinY && event.clientX <= rect.left + threshold
    const nearRight = edges.right && withinY && event.clientX >= rect.right - threshold
    return nearTop || nearBottom || nearLeft || nearRight
  }

  private startMoveSelection(rowIndex: number, colIndex: number, event: MouseEvent) {
    const host = this.getPrimaryContainer()
    if (!host) return false
    const source = this.body.getActiveSelectionRange()
    if (!source) return false
    const geometry = this.computeMoveGeometry(source)
    if (!geometry) return false

    const selectionHeight = source.endRow - source.startRow + 1
    const selectionWidth = source.endCol - source.startCol + 1
    const maxRowStart = Math.max(0, this.totalRowCount - selectionHeight)
    const totalColumns = this.getTotalColumnCount()
    const maxColStart = Math.max(0, totalColumns - selectionWidth)

    const containerRect = this.measureContainerRect()
    if (!containerRect) return false
    this.moveState = {
      source,
      offsetRow: rowIndex - source.startRow,
      offsetCol: colIndex - source.startCol,
      pointerStart: { x: event.clientX, y: event.clientY },
      geometry,
      clientBounds: {
        left: containerRect.left,
        top: containerRect.top,
        right: containerRect.right,
        bottom: containerRect.bottom,
      },
      deltaRow: 0,
      deltaCol: 0,
      maxRowStart,
      maxColStart,
    }

    this.interactionMode = InteractionModeEnum.Moving
    const overlay = this.ensureMoveOverlay()
    overlay.style.display = "block"
    overlay.style.width = `${geometry.width}px`
    overlay.style.height = `${geometry.height}px`
    overlay.style.transform = `translate(${geometry.containerLeft}px, ${geometry.containerTop}px)`
    this.updateCursorStyles()

    event.preventDefault()
    event.stopPropagation()
    this.attachMoveListeners()
    return true
  }

  private computeMoveGeometry(source: SelectionAreaDescriptor | null): MoveSelectionGeometry | null {
    if (!source) return null
    if (!this.getPrimaryContainer()) return null
    const topCell = this.rowController.getCellElement(source.startRow, source.startCol)
    if (!topCell) return null
    const topRect = this.getCellRectRelativeToPrimary(topCell)
    const left = topRect.left
    const top = topRect.top

    const selectionHeight = source.endRow - source.startRow + 1
    const selectionWidth = source.endCol - source.startCol + 1
    const topRowSlot = this.rowController.findRowSlotByDisplayIndex(source.startRow)
    let width = 0
    if (topRowSlot) {
      let lastRight = left
      for (let offset = 0; offset < selectionWidth; offset += 1) {
        const columnIndex = source.startCol + offset
        const cellSlot = this.rowController.findCellSlotByColumnIndex(topRowSlot, columnIndex)
        if (!cellSlot) {
          width = 0
          break
        }
        const rect = this.getCellRectRelativeToPrimary(cellSlot.wrapper)
        const cellRight = rect.right
        lastRight = cellRight
        width = cellRight - left
      }
      if (width <= 0) {
        width = Math.max(0, lastRight - left)
      }
    }
    if (width <= 0) {
      const fallback = topRect.width || 24
      width = selectionWidth * fallback
    }

    const rowHeight = this.rowHeight || topRect.height || 24
    const height = selectionHeight * rowHeight

    const geometry = this.moveGeometryCache
    geometry.containerLeft = left
    geometry.containerTop = top
    geometry.width = width
    geometry.height = height
    geometry.rowHeight = rowHeight
    geometry.avgColumnWidth = width / Math.max(1, selectionWidth)
    return geometry
  }

  private ensureMoveOverlay() {
    const host = this.getPrimaryContainer()
    if (this.moveOverlay) {
      if (host && !this.moveOverlay.isConnected) {
        host.appendChild(this.moveOverlay)
      }
      return this.moveOverlay
    }

    const overlay = document.createElement("div")
    overlay.className = mergeClasses(this.classMap.moveOverlay)
    overlay.style.position = "absolute"
    overlay.style.pointerEvents = "none"
    overlay.style.zIndex = "60"
    overlay.style.border = "2px dashed rgba(37, 99, 235, 0.7)"
    overlay.style.background = "transparent"
    overlay.style.borderRadius = "0"
    overlay.style.boxSizing = "border-box"
    overlay.style.display = "none"
    if (host) {
      host.appendChild(overlay)
    }
    this.moveOverlay = overlay
    return overlay
  }

  private attachMoveListeners() {
    if (this.moveListenersAttached) return
    window.addEventListener("mousemove", this.handleWindowPointerMove)
    window.addEventListener("mouseup", this.handleWindowPointerUp)
    window.addEventListener("keydown", this.handleWindowKeyDown, true)
    this.moveListenersAttached = true
  }

  private detachMoveListeners() {
    if (!this.moveListenersAttached) return
    window.removeEventListener("mousemove", this.handleWindowPointerMove)
    window.removeEventListener("mouseup", this.handleWindowPointerUp)
    window.removeEventListener("keydown", this.handleWindowKeyDown, true)
    this.moveListenersAttached = false
  }

  private handleWindowPointerMove = (event: MouseEvent) => {
    if (!this.moveState) return
    event.preventDefault()
    this.updateMoveDrag(event)
  }

  private handleWindowPointerUp = (event: MouseEvent) => {
    if (!this.moveState) return
    event.preventDefault()
    this.finishMoveSelection(true)
    this.setMoveCursorActive(false)
  }

  private handleWindowKeyDown = (event: KeyboardEvent) => {
    if (!this.moveState) return
    if (event.key === "Escape" || event.key === "Esc") {
      event.preventDefault()
      this.cancelMoveSelection()
      this.setMoveCursorActive(false)
    }
  }

  private updateMoveDrag(event: MouseEvent) {
    const state = this.moveState
    if (!state) return
    const rowStep = state.geometry.rowHeight || 1
    const colStep = state.geometry.avgColumnWidth || 1

    let rowDelta = 0
    if (rowStep > 0) {
      rowDelta = Math.round((event.clientY - state.pointerStart.y) / rowStep)
    }
    let targetRowStart = state.source.startRow + rowDelta
    targetRowStart = Math.max(0, Math.min(state.maxRowStart, targetRowStart))
    const normalizedRowDelta = targetRowStart - state.source.startRow

    let colDelta = 0
    if (colStep > 0) {
      colDelta = Math.round((event.clientX - state.pointerStart.x) / colStep)
    }
    let targetColStart = state.source.startCol + colDelta
    targetColStart = Math.max(0, Math.min(state.maxColStart, targetColStart))
    const normalizedColDelta = targetColStart - state.source.startCol

    if (normalizedRowDelta === state.deltaRow && normalizedColDelta === state.deltaCol) {
      return
    }

    state.deltaRow = normalizedRowDelta
    state.deltaCol = normalizedColDelta
    this.positionMoveOverlay(state)
  }

  private positionMoveOverlay(state: MoveSelectionState) {
    if (!this.moveOverlay) return
    const overlay = this.moveOverlay
    const targetRow = state.source.startRow + state.deltaRow
    const targetCol = state.source.startCol + state.deltaCol

    const baseTop = state.geometry.containerTop + state.deltaRow * state.geometry.rowHeight
    let baseLeft = state.geometry.containerLeft + state.deltaCol * state.geometry.avgColumnWidth

    const anchorCell = this.rowController.getCellElement(targetRow, targetCol)
    if (anchorCell) {
      const anchorRect = this.getCellRectRelativeToPrimary(anchorCell)
      baseLeft = anchorRect.left
    }

    overlay.style.transform = `translate(${baseLeft}px, ${baseTop}px)`
  }

  private finishMoveSelection(commit: boolean) {
    const state = this.moveState
    if (!state) return
    this.detachMoveListeners()
    this.moveState = null
    this.interactionMode = InteractionModeEnum.None
    if (this.moveOverlay) {
      this.moveOverlay.style.display = "none"
    }
    this.updateCursorStyles()
    if (commit) {
      const targetRow = state.source.startRow + state.deltaRow
      const targetCol = state.source.startCol + state.deltaCol
      this.body.moveActiveSelectionTo(targetRow, targetCol)
    }
  }

  private cancelMoveSelection() {
    this.finishMoveSelection(false)
  }

  private getTotalColumnCount() {
    const ariaCount = this.body.ariaColCount.value
    if (typeof ariaCount === "number" && Number.isFinite(ariaCount) && ariaCount > 0) {
      return ariaCount
    }
    const blueprintCount = this.blueprintManager.getBlueprints().length
    return Math.max(this.totalColumnCount, blueprintCount)
  }

  private setMoveCursorActive(active: boolean) {
    if (this.moveCursorActive === active) return
    this.moveCursorActive = active
    this.updateCursorStyles()
  }

  private updateMoveCursor(event: MouseEvent, explicitCell?: HTMLElement | null) {
    if (!this.attachedContainers.size) return
    if (this.interactionMode === InteractionModeEnum.Moving) return
    if (event.buttons & 1) return
    if (!this.body.canMoveActiveSelection()) {
      this.setMoveCursorActive(false)
      return
    }
    if (this.body.isFillDragging()) {
      this.setMoveCursorActive(false)
      return
    }
    const cellElement = explicitCell ?? this.rowController.findAncestorCell(event.target as HTMLElement)
    if (!cellElement) {
      this.setMoveCursorActive(false)
      return
    }
    const rowIndex = Number.parseInt(cellElement.dataset.rowIndex ?? "-1", 10)
    const colIndex = Number.parseInt(cellElement.dataset.colIndex ?? "-1", 10)
    if (rowIndex < 0 || colIndex < 0) {
      this.setMoveCursorActive(false)
      return
    }
    const active = this.isMoveInteractionZone(cellElement, rowIndex, colIndex, event)
    this.setMoveCursorActive(active)
  }

  private measureCellRect(element: HTMLElement): DOMRect {
    let rect: DOMRect | null = null
    const handle = this.measurementQueue.schedule(() => {
      rect = element.getBoundingClientRect()
      return rect
    })
    this.measurementQueue.flush()
    void handle.promise.catch(() => {})
    if (!rect) {
      rect = element.getBoundingClientRect()
    }
    return rect
  }

  private measureContainerRect(): CellRect | null {
    if (!this.attachedContainers.size) {
      return null
    }

    let minLeft = Number.POSITIVE_INFINITY
    let minTop = Number.POSITIVE_INFINITY
    let maxRight = Number.NEGATIVE_INFINITY
    let maxBottom = Number.NEGATIVE_INFINITY

    for (const container of this.attachedContainers) {
      const rect = this.measureElementRect(container)
      if (rect.left < minLeft) minLeft = rect.left
      if (rect.top < minTop) minTop = rect.top
      if (rect.right > maxRight) maxRight = rect.right
      if (rect.bottom > maxBottom) maxBottom = rect.bottom
    }

    if (!Number.isFinite(minLeft) || !Number.isFinite(minTop) || !Number.isFinite(maxRight) || !Number.isFinite(maxBottom)) {
      return null
    }

    const width = Math.max(0, maxRight - minLeft)
    const height = Math.max(0, maxBottom - minTop)
    return {
      left: minLeft,
      top: minTop,
      right: maxRight,
      bottom: maxBottom,
      width,
      height,
    }
  }
}
