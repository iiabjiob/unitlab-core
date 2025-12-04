import type { HeaderRenderableEntry } from "../../types/internal.js"
import type { ImperativeContainerMap, UiTableBodyBindings } from "../bindings.js"
import type { ImperativeColumnUpdatePayload, ImperativeRowUpdatePayload } from "../../viewport/tableViewportController.js"
import { mergeClasses, toDisplayValue } from "../dom.js"
import { resolveBodyClassMap, type ImperativeBodyViewConfig, type ResolvedImperativeBodyClassMap } from "../viewConfig.js"
import type {
  CellSlot,
  ColumnBlueprint,
  ImperativeBodyRegion,
  ImperativeRendererAdapter,
  RowSlot,
} from "../types.js"

type ReadonlySignal<T> = { readonly value: T }

interface HeaderSnapshot {
  entries: Partial<Record<ImperativeBodyRegion, HeaderRenderableEntry[]>>
  order: ImperativeBodyRegion[]
}

interface BlueprintManagerLike {
  rebuild(entriesByRegion: Partial<Record<ImperativeBodyRegion, HeaderRenderableEntry[]>>, order: ImperativeBodyRegion[]): void
  clear(): void
  getBlueprints(): ColumnBlueprint[]
  getBlueprintByKey(key: string): ColumnBlueprint | null
  getBlueprintsForRegion(region: ImperativeBodyRegion): ColumnBlueprint[]
  getColumnIndices(): number[]
}

interface EditorControllerLike {
  open(cellSlot: CellSlot, initialValue: unknown, handlers: {
    commit: (value: unknown) => void
    cancel: () => void
    navigate: (payload: { type: string; shift?: boolean }) => void
  }): void
  close(): void
}

interface CellRendererLike {
  refreshColumnFillersMode(rowSlots: RowSlot[]): void
}

interface RowControllerLike {
  setContainers(containers: ImperativeContainerMap | null): void
  getPrimaryRegion(): ImperativeBodyRegion
  getContainer(): HTMLDivElement | null
  findRowSlotByDisplayIndex(displayIndex: number): RowSlot | null
  findCellSlot(rowSlot: RowSlot, columnKey: string): CellSlot | null
  getRowSlots(): RowSlot[]
  updateRows(payload: ImperativeRowUpdatePayload, gridStyle: Record<string, unknown>, gridBaseClass: string): number
  resetCellClassCache(): void
  dispose(): void
}

interface InteractionControllerLike {
  detach(): void
  dispose(): void
  setContainers(containers: ImperativeContainerMap | null, primaryRegion: ImperativeBodyRegion): void
  setMetrics(metrics: { totalRowCount: number; rowHeight: number; totalColumnCount: number }): void
  reappendOverlay(): void
}

export interface ImperativeControllerSuite {
  blueprintManager: BlueprintManagerLike
  editor: EditorControllerLike
  cellRenderer: CellRendererLike
  rowController: RowControllerLike
  interaction: InteractionControllerLike
  dispose?: () => void
}

export interface ImperativeControllerFactoryContext {
  body: UiTableBodyBindings
  mode: ReadonlySignal<boolean>
  renderer: ImperativeRendererAdapter
  classMap: ResolvedImperativeBodyClassMap
  view?: ImperativeBodyViewConfig
  beginCellEdit: (rowSlot: RowSlot, cellSlot: CellSlot, blueprint: ColumnBlueprint) => void
}

interface ImperativeTableEngineOptions {
  body: UiTableBodyBindings
  mode: ReadonlySignal<boolean>
  headerSnapshot: () => HeaderSnapshot
  renderer: ImperativeRendererAdapter
  view?: ImperativeBodyViewConfig
  createControllers: (context: ImperativeControllerFactoryContext) => ImperativeControllerSuite
}

export class ImperativeTableEngine {
  private readonly body: UiTableBodyBindings
  private readonly mode: ReadonlySignal<boolean>
  private readonly headerSnapshot: () => HeaderSnapshot
  private readonly renderer: ImperativeRendererAdapter
  private readonly classMap: ResolvedImperativeBodyClassMap

  private readonly blueprintManager: BlueprintManagerLike
  private readonly editor: EditorControllerLike
  private readonly cellRenderer: CellRendererLike
  private readonly rowController: RowControllerLike
  private readonly interaction: InteractionControllerLike
  private readonly suiteDispose: (() => void) | undefined

  private totalRowCount = 0
  private rowHeight = 0
  private totalColumnCount = 0

  constructor(options: ImperativeTableEngineOptions) {
    this.body = options.body
    this.mode = options.mode
  this.headerSnapshot = options.headerSnapshot
    this.renderer = options.renderer
    this.classMap = resolveBodyClassMap(options.view?.classMap)

    const suite = options.createControllers({
      body: this.body,
      mode: this.mode,
      renderer: this.renderer,
      classMap: this.classMap,
      view: options.view,
      beginCellEdit: (rowSlot, cellSlot, blueprint) => this.beginCellEdit(rowSlot, cellSlot, blueprint),
    })

    const { blueprintManager, editor, cellRenderer, rowController, interaction, dispose } = suite
    this.blueprintManager = blueprintManager
    this.editor = editor
    this.cellRenderer = cellRenderer
    this.rowController = rowController
    this.interaction = interaction
    this.suiteDispose = dispose
  }

  registerContainer(containers: ImperativeContainerMap | null) {
    this.rowController.setContainers(containers)
    if (!this.hasAnyContainer(containers)) {
      this.interaction.detach()
      this.editor.close()
      this.blueprintManager.clear()
      return
    }
    if (!this.mode.value) return
    const primaryRegion = this.rowController.getPrimaryRegion()
    this.interaction.setContainers(containers, primaryRegion)
  }

  attach(containers: ImperativeContainerMap) {
    this.registerContainer(containers)
  }

  detach() {
    this.registerContainer(null)
  }

  handleRows(payload: ImperativeRowUpdatePayload) {
    this.updateRows(payload)
  }

  handleColumns(payload: ImperativeColumnUpdatePayload) {
    this.updateColumns(payload)
  }

  handleEditCommand(command: { rowIndex: number; key: string }) {
    if (!this.mode.value) return
    if (!this.rowController.getContainer()) return
    const rowSlot = this.rowController.findRowSlotByDisplayIndex(command.rowIndex)
    if (!rowSlot || rowSlot.type !== "data") return
    const cellSlot = this.rowController.findCellSlot(rowSlot, command.key)
    if (!cellSlot || cellSlot.type !== "data" || !cellSlot.wrapper.isConnected) return
    const blueprint = this.blueprintManager.getBlueprintByKey(command.key)
    if (!blueprint) return
    this.beginCellEdit(rowSlot, cellSlot, blueprint)
  }

  dispose() {
    this.interaction.dispose()
    this.rowController.dispose()
    this.editor.close()
    this.blueprintManager.clear()
    this.suiteDispose?.()
  }

  private updateColumns(payload: ImperativeColumnUpdatePayload) {
    if (!this.mode.value) return
    const ariaCount = this.body.ariaColCount.value
    if (typeof ariaCount === "number" && Number.isFinite(ariaCount) && ariaCount > 0) {
      this.totalColumnCount = ariaCount
    } else {
      const visibleCount = payload.snapshot.visibleColumns.length
      if (visibleCount > this.totalColumnCount) {
        this.totalColumnCount = visibleCount
      }
    }

    const snapshot = this.resolveHeaderSnapshot()
    this.blueprintManager.rebuild(snapshot.entries, snapshot.order)
    this.rowController.resetCellClassCache()

    this.interaction.setMetrics({
      totalRowCount: this.totalRowCount,
      rowHeight: this.rowHeight,
      totalColumnCount: this.getTotalColumnCount(),
    })
  }

  private updateRows(payload: ImperativeRowUpdatePayload) {
    if (!this.mode.value) return
    if (!this.rowController.getContainer()) return
    this.ensureBlueprints()

    this.totalRowCount = payload.totalRowCount
    this.rowHeight = payload.rowHeight

    this.cellRenderer.refreshColumnFillersMode(this.rowController.getRowSlots())

    const gridStyle = this.body.rowGridStyle.value as Record<string, unknown>
    const hoverable = this.body.isHoverableTable.value
    const gridBaseClass = mergeClasses(
      this.classMap.rowGrid,
      this.body.bodyRowClass.value,
      hoverable ? "ui-table__row-layer--hoverable" : "",
    )

    this.rowController.updateRows(payload, gridStyle, gridBaseClass)
    this.interaction.reappendOverlay()

    this.interaction.setMetrics({
      totalRowCount: this.totalRowCount,
      rowHeight: this.rowHeight,
      totalColumnCount: this.getTotalColumnCount(),
    })
  }

  private ensureBlueprints() {
    if (this.blueprintManager.getBlueprints().length) return
    const snapshot = this.resolveHeaderSnapshot()
    this.blueprintManager.rebuild(snapshot.entries, snapshot.order)
  }

  private getTotalColumnCount() {
    const ariaCount = this.body.ariaColCount.value
    if (typeof ariaCount === "number" && Number.isFinite(ariaCount) && ariaCount > 0) {
      return ariaCount
    }
    return Math.max(this.totalColumnCount, this.blueprintManager.getBlueprints().length)
  }

  private resolveHeaderSnapshot(): HeaderSnapshot {
    const snapshot = this.headerSnapshot()
    const entries = { ...snapshot.entries }
    if (!entries.main) {
      entries.main = this.body.headerRenderableEntries.value
    }
  const order: ImperativeBodyRegion[] = snapshot.order.length ? [...snapshot.order] : (["main"] as ImperativeBodyRegion[])
    if (!order.includes("main")) {
      order.push("main")
    }
    return { entries, order }
  }

  private hasAnyContainer(containers: ImperativeContainerMap | null): boolean {
    if (!containers) return false
    return Boolean(containers["pinned-left"] || containers.main || containers["pinned-right"])
  }

  private beginCellEdit(rowSlot: RowSlot, cellSlot: CellSlot, blueprint: ColumnBlueprint) {
    if (cellSlot.type === "selection" || cellSlot.type === "index") return
    const column = blueprint.column
    if (!this.body.isColumnEditable(column)) return

    const colKey = column.key
    const originalIndex = rowSlot.originalIndex ?? null
    const rowData = rowSlot.rowData
    const initialValue = cellSlot.value != null ? toDisplayValue(cellSlot.value) : ""

    this.body.handleCellEditingChange(true, colKey, originalIndex)
    this.editor.open(cellSlot, initialValue, {
      commit: value => {
        const previous = rowData?.[colKey]
        if (rowData == null || previous !== value) {
          this.body.onCellEdit({
            rowIndex: rowSlot.rowIndex,
            originalRowIndex: originalIndex ?? undefined,
            displayRowIndex: rowSlot.displayIndex,
            key: colKey,
            value,
            row: rowData,
          })
        }
        this.body.handleCellEditingChange(false, colKey, originalIndex)
      },
      cancel: () => {
        this.body.handleCellEditingChange(false, colKey, originalIndex)
      },
      navigate: action => {
        const payload: {
          rowIndex: number
          key: string
          colIndex?: number
          shift?: boolean
          handled?: boolean
          direction?: "up" | "down"
        } = {
          rowIndex: rowSlot.displayIndex,
          key: colKey,
          colIndex: cellSlot.columnIndex,
          shift: action.shift,
          handled: false,
        }
        if (action.type === "enter") {
          payload.direction = action.shift ? "up" : "down"
        }
        this.body.focusNextCell(payload)
      },
    })
  }
}

export interface ImperativeBodyController {
  registerContainer: (containers: ImperativeContainerMap | null) => void
  handleRows: (payload: ImperativeRowUpdatePayload) => void
  handleColumns: (payload: ImperativeColumnUpdatePayload) => void
  handleEditCommand: (command: { rowIndex: number; key: string }) => void
}

export type { ImperativeRendererAdapter } from "../types.js"
export type { ImperativeBodyViewConfig, ImperativeBodyClassMap } from "../viewConfig.js"
export type { HeaderSnapshot }
