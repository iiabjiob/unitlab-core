import { mergeClasses, setVisibility, applyStyle } from "../../dom.js"
import {
  createMeasurementQueue,
  type MeasurementQueue,
} from "../../../runtime/measurementQueue.js"
import type { ImperativeContainerMap, UiTableBodyBindings } from "../../bindings.js"
import type {
  CellSlot,
  ImperativeBodyRegion,
  ImperativeSelectionSnapshot,
  RowSlot,
  RowSlotRegionState,
} from "../../types.js"
import type { ImperativeRowUpdatePayload } from "../../../viewport/tableViewportController.js"
import type { ResolvedImperativeBodyClassMap } from "../../viewConfig.js"
import type { CellRenderer } from "./CellRenderer.js"
import type { BlueprintManager } from "./BlueprintManager.js"

declare global {
  interface Window {
    __UNITLAB_TABLE_DEBUG__?: boolean
  }
}

const REGION_ORDER: ImperativeBodyRegion[] = ["pinned-left", "main", "pinned-right"]

/**
 * Owns row-slot creation, teardown, and per-row rendering.
 */
export class RowController {
  private containers: ImperativeContainerMap = {}
  private primaryRegion: ImperativeBodyRegion = "main"
  private readonly rowSlots: RowSlot[] = []
  private readonly rowIndexScratch: number[] = []
  private readonly columnIndexScratch: number[] = []
  private readonly rectScratch = {
    left: 0,
    top: 0,
    right: 0,
    bottom: 0,
    width: 0,
    height: 0,
  }

  private readonly measurementQueue: MeasurementQueue
  private readonly ownsMeasurementQueue: boolean

  constructor(
    private readonly body: UiTableBodyBindings,
    private readonly classMap: ResolvedImperativeBodyClassMap,
    private readonly cellRenderer: CellRenderer,
    private readonly blueprintManager: BlueprintManager,
    measurementQueue?: MeasurementQueue,
  ) {
    this.measurementQueue = measurementQueue ?? createMeasurementQueue()
    this.ownsMeasurementQueue = !measurementQueue
  }

  setContainers(containers: ImperativeContainerMap | null) {
    if (this.areContainersEqual(containers)) return
    this.detach()
    this.containers = containers ? { ...containers } : {}
    this.primaryRegion = this.resolvePrimaryRegion()
  }

  /**
   * Backwards-compatible single-container setter used by legacy call sites.
   */
  setContainer(container: HTMLDivElement | null) {
    if (container) {
      this.setContainers({ main: container })
    } else {
      this.setContainers(null)
    }
  }

  getContainer(): HTMLDivElement | null {
    return this.getPrimaryContainer()
  }

  getPrimaryRegion(): ImperativeBodyRegion {
    return this.primaryRegion
  }

  private getPrimaryContainer(): HTMLDivElement | null {
    const primary = this.getContainerForRegion(this.primaryRegion)
    if (primary) return primary
    for (const region of REGION_ORDER) {
      const candidate = this.getContainerForRegion(region)
      if (candidate) return candidate
    }
    return null
  }

  detach() {
    for (const slot of this.rowSlots) {
      this.teardownRowSlot(slot)
    }
    this.rowSlots.length = 0
  }

  resetCellClassCache() {
    for (const slot of this.rowSlots) {
      for (const cell of slot.cells) {
        cell.classCache = ""
      }
    }
  }

  getRowSlots(): RowSlot[] {
    return this.rowSlots
  }

  updateRows(
    payload: ImperativeRowUpdatePayload,
    gridStyle: Record<string, unknown>,
    gridBaseClass: string,
  ): number {
    const primaryContainer = this.getPrimaryContainer()
    if (!primaryContainer) return 0

    const pool = payload.pool

    let selectionSnapshot: ImperativeSelectionSnapshot | null = null
    let hasCutPreview = this.body.hasCutPreview()
    if (typeof this.body.buildSelectionSnapshot === "function") {
      const builder = this.body.buildSelectionSnapshot as
        | ((payload: { rowIndices: number[]; columnIndices: number[] }) => ImperativeSelectionSnapshot | null | undefined)
        | undefined
      if (builder) {
        let columnIndices = this.blueprintManager.getColumnIndices()
        if (!columnIndices.length) {
          const scratch = this.columnIndexScratch
          scratch.length = 0
          const blueprints = this.blueprintManager.getBlueprints()
          for (let index = 0; index < blueprints.length; index += 1) {
            scratch.push(blueprints[index].binding.columnIndex)
          }
          columnIndices = scratch
        }

        const rowIndices = this.rowIndexScratch
        rowIndices.length = 0
        for (let index = 0; index < pool.length; index += 1) {
          const pooled = pool[index]
          if (!pooled.entry) continue
          if (this.body.isGroupRowEntry(pooled.entry)) continue
          rowIndices.push(pooled.displayIndex)
        }

        selectionSnapshot = builder({ rowIndices, columnIndices }) ?? null
        hasCutPreview = selectionSnapshot?.hasCutPreview ?? hasCutPreview
      }
    }

    let updatedNodes = 0

    for (let index = 0; index < pool.length; index += 1) {
      const pooled = pool[index]
      const slot = this.ensureRowSlot(index)

      slot.rowIndex = pooled.rowIndex
      slot.displayIndex = pooled.displayIndex
      slot.originalIndex = pooled.entry?.originalIndex ?? null

      const layerStyle = this.body.rowLayerStyle(pooled) as Record<string, unknown>
      this.applyLayerStyle(slot, layerStyle)

      if (!pooled.entry) {
        slot.type = "empty"
        this.setRowVisibility(slot, false)
        continue
      }

      this.setRowVisibility(slot, true)

      if (this.body.isGroupRowEntry(pooled.entry)) {
        slot.type = "group"
        updatedNodes += this.renderGroupRow(slot, pooled.entry.row)
      } else {
        slot.type = "data"
        updatedNodes += this.renderDataRow(
          slot,
          pooled,
          gridStyle,
          gridBaseClass,
          selectionSnapshot,
          hasCutPreview,
        )
      }
    }

    for (let index = pool.length; index < this.rowSlots.length; index += 1) {
      this.teardownRowSlotAt(index)
    }

    if (this.rowSlots.length > pool.length) {
      this.rowSlots.length = pool.length
    }

    return updatedNodes
  }

  findRowSlotByDisplayIndex(displayIndex: number): RowSlot | null {
    for (const slot of this.rowSlots) {
      if (slot.type !== "data") continue
      if (slot.displayIndex !== displayIndex) continue
      return slot
    }
    return null
  }

  findCellSlot(rowSlot: RowSlot, columnKey: string): CellSlot | null {
    for (const cell of rowSlot.cells) {
      if (cell.columnKey === columnKey) {
        return cell
      }
    }
    return null
  }

  findCellSlotByColumnIndex(rowSlot: RowSlot, columnIndex: number): CellSlot | null {
    for (const cell of rowSlot.cells) {
      if (cell.columnIndex === columnIndex) {
        return cell
      }
    }
    return null
  }

  getCellElement(rowIndex: number, columnIndex: number): HTMLElement | null {
    const rowSlot = this.findRowSlotByDisplayIndex(rowIndex)
    if (!rowSlot) return null
    const cellSlot = this.findCellSlotByColumnIndex(rowSlot, columnIndex)
    if (!cellSlot || !cellSlot.wrapper.isConnected) return null
    return cellSlot.wrapper
  }

  getCellRectWithinContainer(cell: HTMLElement) {
    let result: typeof this.rectScratch | null = null
    const handle = this.measurementQueue.schedule(() => {
      result = this.readCellRectWithinContainer(cell)
      return result
    })
    this.measurementQueue.flush()
    void handle.promise.catch(() => {})
    if (result) {
      return result
    }
    return this.readCellRectWithinContainer(cell)
  }

  findAncestorCell(element: HTMLElement | null): HTMLElement | null {
    let current: HTMLElement | null = element
    while (current) {
      if (current.dataset?.rowIndex != null && current.dataset?.colKey != null) {
        return current
      }
      current = current.parentElement
    }
    return null
  }

  private ensureRowSlot(index: number): RowSlot {
    const existing = this.rowSlots[index]
    if (existing) return existing

    const regions: Partial<Record<ImperativeBodyRegion, RowSlotRegionState>> = {}

    for (const region of REGION_ORDER) {
      const container = this.getContainerForRegion(region)
      if (!container) continue

      const layer = document.createElement("div")
      layer.className = mergeClasses(this.classMap.rowLayer)
      layer.setAttribute("role", "presentation")
      layer.dataset.region = region

      const grid = document.createElement("div")
      grid.className = mergeClasses(this.classMap.rowGrid)
      grid.dataset.region = region

      layer.appendChild(grid)
      container.appendChild(layer)

      regions[region] = {
        region,
        layer,
        layerStyleCache: new Map(),
        grid,
        gridStyleCache: new Map(),
      }
    }

    const primaryRegion = this.resolvePrimaryRegion(regions)
    const primaryState = regions[primaryRegion]
    if (!primaryState) {
      throw new Error("Missing container for primary region")
    }

    const groupRow = document.createElement("div")
    groupRow.className = mergeClasses(this.classMap.groupRow, this.body.groupRowClass.value)
    const groupCell = document.createElement("span")
    groupCell.className = mergeClasses(this.classMap.groupCell, this.body.groupCellClass.value)
    groupCell.setAttribute("role", "gridcell")
    groupCell.tabIndex = 0
    const caret = document.createElement("span")
    caret.className = mergeClasses(this.classMap.groupCaret, this.body.groupCaretClass.value)
    caret.setAttribute("aria-hidden", "true")
    const label = document.createElement("span")
    label.className = mergeClasses(this.classMap.groupLabel)
    const count = document.createElement("span")
    count.className = mergeClasses(this.classMap.groupCount)
    groupCell.appendChild(caret)
    groupCell.appendChild(label)
    groupCell.appendChild(count)
    groupRow.appendChild(groupCell)

    groupCell.addEventListener("click", event => {
      event.stopPropagation()
      const key = groupRow.dataset.groupKey
      if (key) this.body.toggleGroupRow(key)
    })
    groupCell.addEventListener("keydown", event => {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault()
        const key = groupRow.dataset.groupKey
        if (key) this.body.toggleGroupRow(key)
      }
    })

    primaryState.layer.appendChild(groupRow)

    const slot: RowSlot = {
      regions,
      primaryRegion,
      layer: primaryState.layer,
      layerStyleCache: primaryState.layerStyleCache,
      grid: primaryState.grid,
      gridStyleCache: primaryState.gridStyleCache,
      groupRow,
      groupCell,
      groupCellStyleCache: new Map(),
      groupCaret: caret,
      groupLabel: label,
      groupCount: count,
      type: "empty",
      cells: [],
      rowIndex: -1,
      displayIndex: -1,
      originalIndex: null,
      rowData: null,
      gridClassName: "",
    }

    this.rowSlots[index] = slot
    return slot
  }

  private teardownRowSlotAt(index: number) {
    const slot = this.rowSlots[index]
    if (!slot) return
    this.teardownRowSlot(slot)
  }

  private teardownRowSlot(slot: RowSlot) {
    slot.cells.forEach(cell => this.cellRenderer.teardownCellSlot(cell))
    slot.cells.length = 0

    for (const region of REGION_ORDER) {
      const state = slot.regions[region]
      if (!state) continue
      if (state.layer.parentElement) {
        state.layer.parentElement.removeChild(state.layer)
      }
    }

    slot.regions = {}
  }

  private renderGroupRow(slot: RowSlot, row: any): number {
    const level = Number.isFinite(row?.level) ? Number(row.level) : 0
    const style = this.body.groupCellStyle(level) as Record<string, unknown>
    applyStyle(slot.groupCell, slot.groupCellStyleCache, style)
    slot.groupLabel.textContent = row?.value != null ? String(row.value) : ""
    slot.groupCount.textContent = `(${row?.size ?? 0})`
    slot.groupRow.dataset.groupKey = String(row?.key ?? "")
    slot.groupCaret.classList.toggle("expanded", this.body.isGroupExpanded(String(row?.key ?? "")))
    slot.groupRow.style.display = "block"

    for (const region of REGION_ORDER) {
      const state = slot.regions[region]
      if (!state) continue
      state.grid.style.display = "none"
    }

    slot.rowData = null
    return 1
  }

  private renderDataRow(
    slot: RowSlot,
    pooled: ImperativeRowUpdatePayload["pool"][number],
    gridStyle: Record<string, unknown>,
    gridBaseClass: string,
    selectionSnapshot: ImperativeSelectionSnapshot | null,
    hasCutPreview: boolean,
  ): number {
    let mutations = 0
    const rowData = pooled.entry?.row ?? {}
    slot.rowData = rowData

    const rowModifierRaw = this.body.rowGridClass(rowData)
    const rowModifier = typeof rowModifierRaw === "string" ? rowModifierRaw.trim() : ""
    const nextGridClass = rowModifier ? `${gridBaseClass} ${rowModifier}` : gridBaseClass

    this.applyGridState(slot, gridStyle, nextGridClass)
    slot.groupRow.style.display = "none"

    const blueprints = this.blueprintManager.getBlueprints()
    const blueprintCount = blueprints.length
    for (let i = 0; i < blueprintCount; i += 1) {
      const blueprint = blueprints[i]
      const regionState = slot.regions[blueprint.region]
      if (!regionState) continue
      mutations += this.cellRenderer.renderCell(
        slot,
        regionState,
        i,
        blueprint,
        pooled,
        selectionSnapshot,
        hasCutPreview,
      )
    }

    if (slot.cells.length > blueprintCount) {
      for (let i = blueprintCount; i < slot.cells.length; i += 1) {
        const excess = slot.cells[i]
        if (excess) {
          this.cellRenderer.teardownCellSlot(excess)
        }
      }
      slot.cells.length = blueprintCount
    }

    return mutations
  }

  private applyLayerStyle(slot: RowSlot, style: Record<string, unknown>) {
    for (const region of REGION_ORDER) {
      const state = slot.regions[region]
      if (!state) continue
      applyStyle(state.layer, state.layerStyleCache, style)
    }
  }

  private setRowVisibility(slot: RowSlot, visible: boolean) {
    for (const region of REGION_ORDER) {
      const state = slot.regions[region]
      if (!state) continue
      setVisibility(state.layer, visible)
    }
  }

  private applyGridState(slot: RowSlot, gridStyle: Record<string, unknown>, gridBaseClass: string) {
    if (slot.gridClassName !== gridBaseClass) {
      slot.gridClassName = gridBaseClass
    }

    const templates = this.resolveGridTemplates()
    const blueprintCounts: Partial<Record<ImperativeBodyRegion, number>> = {}
    for (const region of REGION_ORDER) {
      blueprintCounts[region] = this.blueprintManager.getBlueprintsForRegion(region).length
    }

    for (const region of REGION_ORDER) {
      const state = slot.regions[region]
      if (!state) continue
      const template = templates[region]
      const style = template ? { ...gridStyle, gridTemplateColumns: template } : { ...gridStyle }
      applyStyle(state.grid, state.gridStyleCache, style)
      state.grid.className = gridBaseClass
      if (state.grid.style.paddingLeft) {
        state.grid.style.paddingLeft = ""
      }
      if (state.grid.style.paddingRight) {
        state.grid.style.paddingRight = ""
      }
      const hasColumns = (blueprintCounts[region] ?? 0) > 0
      state.grid.style.display = hasColumns ? "grid" : "none"
    }
  }

  private resolveGridTemplates(): Partial<Record<ImperativeBodyRegion, string>> {
    return {
      "pinned-left": this.body.gridTemplateColumnsPinnedLeft.value,
      main: this.body.gridTemplateColumnsMain.value || this.body.gridTemplateColumns.value,
      "pinned-right": this.body.gridTemplateColumnsPinnedRight.value,
    }
  }

  private areContainersEqual(next: ImperativeContainerMap | null): boolean {
    if (!next) {
      for (const region of REGION_ORDER) {
        if (this.containers?.[region]) return false
      }
      return true
    }
    for (const region of REGION_ORDER) {
      if ((this.containers?.[region] ?? null) !== (next[region] ?? null)) {
        return false
      }
    }
    return true
  }

  private resolvePrimaryRegion(source?: Partial<Record<ImperativeBodyRegion, unknown>>): ImperativeBodyRegion {
    const lookup = source ?? this.containers
    if (lookup.main) return "main"
    if (lookup["pinned-left"]) return "pinned-left"
    if (lookup["pinned-right"]) return "pinned-right"
    return "main"
  }

  getContainerForRegion(region: ImperativeBodyRegion): HTMLDivElement | null {
    return this.containers?.[region] ?? null
  }

  private getContainerForCell(cell: HTMLElement): HTMLDivElement | null {
    const regionKey = (cell.dataset?.region as ImperativeBodyRegion | undefined) ?? this.primaryRegion
    return this.getContainerForRegion(regionKey) ?? this.getPrimaryContainer()
  }

  private readCellRectWithinContainer(cell: HTMLElement) {
    const scratch = this.rectScratch
    const rect = cell.getBoundingClientRect()
    scratch.left = rect.left
    scratch.top = rect.top
    scratch.right = rect.right
    scratch.bottom = rect.bottom
    scratch.width = rect.width
    scratch.height = rect.height

    const container = this.getContainerForCell(cell)
    if (!container) {
      return scratch
    }

    const containerRect = container.getBoundingClientRect()
    const hasStickyLeft = cell.classList.contains("ui-table-cell--sticky-left") ||
      cell.classList.contains("ui-table__selection-cell")
    const hasStickyRight = cell.classList.contains("ui-table-cell--sticky-right")
    const region = (cell.dataset?.region as ImperativeBodyRegion | undefined) ?? this.primaryRegion
    const addScrollLeft = !(hasStickyLeft || hasStickyRight) && region === this.primaryRegion
    const left = rect.left - containerRect.left + (addScrollLeft ? container.scrollLeft : 0)
    const top = rect.top - containerRect.top + container.scrollTop
    const width = rect.width
    const height = rect.height

    scratch.left = left
    scratch.top = top
    scratch.right = left + width
    scratch.bottom = top + height
    scratch.width = width
    scratch.height = height

    return scratch
  }

  dispose() {
    this.detach()
    if (this.ownsMeasurementQueue) {
      this.measurementQueue.dispose()
    }
  }
}
