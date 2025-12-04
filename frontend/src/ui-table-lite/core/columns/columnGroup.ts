import type { UiTableColumn, UiTableColumnGroupDef } from "../types/column"
import {
  type CreateWritableSignal,
  type WritableSignal,
  createWritableSignal,
} from "../runtime/signals"

interface ColumnGroupOptions {
  createSignal?: CreateWritableSignal<boolean>
}

type GroupDefChild = UiTableColumn | UiTableColumnGroupDef

type ColumnGroupChild = ColumnGroup | UiTableColumn

function isGroupDef(candidate: GroupDefChild): candidate is UiTableColumnGroupDef {
  return typeof (candidate as UiTableColumnGroupDef)?.groupId === "string" && Array.isArray((candidate as UiTableColumnGroupDef)?.children)
}

const defaultCreateBooleanSignal: CreateWritableSignal<boolean> = initial => createWritableSignal(initial)

export function getColumnKey(column: UiTableColumn): string {
  return (column as any).id ?? column.key ?? (column as any).field ?? column.label
}

export function isColumnGroup(candidate: ColumnGroupChild | ProvidedColumnGroup): candidate is ColumnGroup {
  return candidate instanceof ColumnGroup
}

export function isProvidedColumnGroup(candidate: ProvidedColumnGroup | UiTableColumn): candidate is ProvidedColumnGroup {
  return candidate instanceof ProvidedColumnGroup
}

export class ColumnGroup {
  private readonly def: UiTableColumnGroupDef
  private readonly parent?: ColumnGroup
  private readonly expanded: WritableSignal<boolean>
  private children: ColumnGroupChild[] = []
  private providedGroup: ProvidedColumnGroup | null = null

  constructor(def: UiTableColumnGroupDef, parent?: ColumnGroup, options?: ColumnGroupOptions) {
    this.def = def
    this.parent = parent
    const initialExpanded = def.expandable === false ? true : def.expanded ?? true
    const createSignal = options?.createSignal ?? defaultCreateBooleanSignal
    this.expanded = createSignal(initialExpanded)
    this.children = []
  }

  initializeChildren(children: ColumnGroupChild[]) {
    this.children = children
    this.providedGroup = new ProvidedColumnGroup(this, this.parent?.getProvidedColumnGroup() ?? null)
  }

  getColGroupDef(): UiTableColumnGroupDef {
    return this.def
  }

  getGroupId(): string {
    return this.def.groupId
  }

  getProvidedColumnGroup(): ProvidedColumnGroup | null {
    return this.providedGroup
  }

  getParent(): ColumnGroup | undefined {
    return this.parent
  }

  getChildren(): ColumnGroupChild[] {
    return this.children
  }

  getDisplayedChildren(): ColumnGroupChild[] {
    if (!this.isExpandable()) {
      return this.children
    }
    return this.isExpanded() ? this.children : []
  }

  getLeafColumns(): UiTableColumn[] {
    const leaves: UiTableColumn[] = []
    for (const child of this.children) {
      if (isColumnGroup(child)) {
        leaves.push(...child.getLeafColumns())
      } else {
        leaves.push(child)
      }
    }
    return leaves
  }

  getDisplayedLeafColumns(): UiTableColumn[] {
    const leaves: UiTableColumn[] = []
    for (const child of this.getDisplayedChildren()) {
      if (isColumnGroup(child)) {
        leaves.push(...child.getDisplayedLeafColumns())
      } else {
        leaves.push(child)
      }
    }
    return leaves
  }

  isResizable(): boolean {
    return this.getLeafColumns().some(column => typeof column.width === "number")
  }

  isExpandable(): boolean {
    if (this.def.expandable === false) {
      return false
    }
    return this.children.length > 0
  }

  isExpanded(): boolean {
    if (!this.isExpandable()) {
      return true
    }
    return this.expanded.value
  }

  setExpanded(value: boolean) {
    if (!this.isExpandable()) return
    if (this.expanded.value === value) return
    this.expanded.value = value
  }

  toggleExpanded() {
    this.setExpanded(!this.isExpanded())
  }

  getExpandedSignal(): WritableSignal<boolean> {
    return this.expanded
  }

  isPadding(): boolean {
    if (typeof this.def.paddingLevel === "number") return true
    return !this.def.headerName
  }

  getPaddingLevel(): number {
    if (typeof this.def.paddingLevel === "number") {
      return this.def.paddingLevel
    }
    if (!this.parent) return 0
    return this.parent.getPaddingLevel() + 1
  }

  isColumn(): false {
    return false
  }
}

export class ProvidedColumnGroup {
  private readonly original: ColumnGroup
  private readonly parent: ProvidedColumnGroup | null

  constructor(group: ColumnGroup, parent: ProvidedColumnGroup | null) {
    this.original = group
    this.parent = parent
  }

  getOriginalParent(): ProvidedColumnGroup | null {
    return this.parent
  }

  getLevel(): number {
    return this.parent ? this.parent.getLevel() + 1 : 0
  }

  isPadding(): boolean {
    return this.original.isPadding()
  }

  isExpandable(): boolean {
    return this.original.isExpandable()
  }

  isExpanded(): boolean {
    return this.original.isExpanded()
  }

  getGroupId(): string {
    return this.original.getGroupId()
  }

  getChildren(): (ProvidedColumnGroup | UiTableColumn)[] {
    const source = this.original.getDisplayedChildren()
    return source.map(child => (isColumnGroup(child) ? new ProvidedColumnGroup(child, this) : child))
  }

  getColGroupDef(): UiTableColumnGroupDef {
    return this.original.getColGroupDef()
  }

  getLeafColumns(): UiTableColumn[] {
    return this.original.getDisplayedLeafColumns()
  }

  isColumn(): false {
    return false
  }
}

export interface ColumnTreeBuilderResult {
  roots: ColumnGroup[]
  unusedColumns: UiTableColumn[]
}

export interface ColumnGroupRenderNode {
  group: ColumnGroup
  level: number
  startLine: number
  endLine: number
}

export type ColumnGroupRenderRows = ColumnGroupRenderNode[][]

export interface ColumnTreeBuilderOptions {
  createSignal?: CreateWritableSignal<boolean>
}

export function buildColumnGroupRenderRows(groups: ColumnGroup[], trackStartLookup: Map<string, number>): ColumnGroupRenderRows {
  const rows: ColumnGroupRenderNode[][] = []

  const traverse = (group: ColumnGroup, level: number) => {
    const leafColumns = group.getLeafColumns()
    if (!leafColumns.length) return
    const lines = leafColumns
      .map(column => trackStartLookup.get(getColumnKey(column)))
      .filter((value): value is number => value != null)
      .sort((a, b) => a - b)
    if (!lines.length) return
    const startLine = lines[0]
    const endLine = (lines[lines.length - 1] ?? startLine) + 1
    if (!rows[level]) {
      rows[level] = []
    }
    rows[level].push({
      group,
      level,
      startLine,
      endLine,
    })
    if (!group.isExpanded()) return
    for (const child of group.getDisplayedChildren()) {
      if (isColumnGroup(child)) {
        traverse(child, level + 1)
      }
    }
  }

  for (const group of groups) {
    traverse(group, 0)
  }

  return rows
}

export function buildColumnTree(
  columns: UiTableColumn[],
  groupDefs: UiTableColumnGroupDef[],
  options: ColumnTreeBuilderOptions = {},
): ColumnTreeBuilderResult {
  const columnMap = new Map<string, UiTableColumn>()
  const usedColumns = new Set<string>()
  const createSignal = options.createSignal ?? defaultCreateBooleanSignal

  columns.forEach(column => {
    columnMap.set(getColumnKey(column), column)
  })

  const buildGroup = (def: UiTableColumnGroupDef, parent?: ColumnGroup): ColumnGroup => {
    const group = new ColumnGroup(def, parent, { createSignal })
    const children: ColumnGroupChild[] = []
    for (const child of def.children) {
      if (isGroupDef(child)) {
        const nested = buildGroup(child, group)
        children.push(nested)
      } else {
        const key = getColumnKey(child)
        const column = columnMap.get(key) ?? child
        children.push(column)
        usedColumns.add(key)
      }
    }
    group.initializeChildren(children)
    return group
  }

  const roots: ColumnGroup[] = []
  for (const def of groupDefs) {
    roots.push(buildGroup(def, undefined))
  }

  const unusedColumns: UiTableColumn[] = []
  for (const column of columns) {
    const key = getColumnKey(column)
    if (!usedColumns.has(key)) {
      unusedColumns.push(column)
    }
  }

  return {
    roots,
    unusedColumns,
  }
}
