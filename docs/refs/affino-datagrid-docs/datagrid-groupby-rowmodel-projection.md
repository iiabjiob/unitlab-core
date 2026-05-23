# DataGrid GroupBy RowModel Projection

Updated: `2026-02-21`

This document defines canonical GroupBy architecture for `@affino/datagrid-core`.

## Core Principle

GroupBy is a **row-model transformation** with **tree projection**, not a UI/header-only feature.

- UI adapters only render projection output.
- Group logic stays in RowModel layer.
- DataGrid remains a grid with projected virtual rows.

## Concept

GroupBy converts a flat row set into a deterministic hierarchy:

- group rows are virtual rows
- leaf rows are data rows
- hierarchy is expandable/collapsible

## Canonical Row Kinds

```ts
type DataGridRowKind = "group" | "leaf"

interface DataGridGroupRowNode {
  kind: "group"
  rowKey: string
  groupField: string
  groupValue: string
  level: number
  expanded: boolean
  childrenCount: number
}

interface DataGridLeafRowNode<T> {
  kind: "leaf"
  rowKey: string | number
  data: T
}
```

Rule: group rows are rows (not containers).

## Pipeline Order

```
raw rows
  -> optional filter
  -> optional sort
  -> groupBy(fields[])
  -> aggregate(groupMeta.aggregates)
  -> flattenTree(expandedState)
  -> virtualization window
```

Critical ordering:

- Grouping runs before flattening.
- Aggregation runs after grouping and before pagination/visible materialization.
- Virtualization runs after flattening.
- Selection/range operations run on flattened rows.

## Aggregation Model

```ts
interface DataGridAggregationModel<T = unknown> {
  columns: readonly DataGridAggregationColumnSpec<T>[]
  basis?: "filtered" | "source"
}
```

- Aggregation is computed only from leaf rows.
- Aggregates are stored in `row.groupMeta.aggregates`.
- Source row payload (`row.data`) is never mutated by aggregation.
- Default basis is `"filtered"` (totals for current filtered view).
- `"source"` basis keeps the same projected group/tree structure but aggregates over full source leaves.
- `countNonNull` is evaluated after column value normalization (`field` read + optional `coerce`).
- `first` / `last` are evaluated in encountered projection order (not semantic order by any external key).

## Runtime Aggregation API

- `setAggregationModel(model | null): void`
- `getAggregationModel(): DataGridAggregationModel | null`

Behavior:

- Client row model: updates aggregates reactively without runtime reset.
- TreeData path/parent: aggregates are cached in tree projection cache and remain stable on collapse/expand.
- Server/data-source models: API is accepted and reflected by model state contract (no client-side aggregate computation).

## `setRows` vs `patchRows`

- `setRows(...)`:
  - full source replacement,
  - full projection recompute.
- `patchRows(...)`:
  - partial row updates by `rowId`,
  - field-aware invalidation (`filter/sort/group/aggregate`),
  - Excel-like freeze by default (`recomputeSort/filter/group = false` unless explicitly enabled in patch options).

Caveat for no-recompute policies:

- If a stage is blocked (for example, `recomputeGroup: false`), projection may be intentionally stale until explicit recompute (`refresh()` or next allowed recompute cycle).
- To reapply current sort/filter/group after edit-burst patches, call `refresh()` (or use patch options with `recompute*: true`).

## GroupBy Spec

```ts
interface DataGridGroupBySpec {
  fields: string[]
  expandedByDefault?: boolean
}
```

RowModel baseline contract:

- `setGroupBy(spec: DataGridGroupBySpec | null): void`
- `null` means plain non-grouped grid
- input rows are never mutated, only projected

## Expansion Contract

- `toggleGroup(groupKey: string): void`
- `groupKey` must be stable
- expansion state is stored separately from source rows
- snapshot surface carries `groupExpansion` (`expandedByDefault`, `toggledGroupKeys`)

## Selection Semantics (default)

- selecting group row selects only the group row
- shift range uses flattened row order
- keyboard shift-extension and additive cell ranges keep group rows as visible flattened rows in app selection state
- row selection may focus/select visible group row ids and reconciles against the current flattened projection
- group expansion changes mark virtual selections stale; row selection reconciliation removes hidden descendant row ids while preserving visible group row ids
- no implicit child selection by default
- optional policy flag may enable group-to-children behavior
- app clipboard copy/cut, paste targets, clear/delete, and fill source/target ranges over group rows are blocked unless a future server operation explicitly defines group-row export or mutation semantics
- server-backed grouped placeholder rows follow the same group-row blocking rule; they must not fall back to local leaf-only mutation or clipboard reads
- local cell mutations never silently apply to only the leaf subset of a range that includes group rows

Core helper for adapters:

- `createGridSelectionContextFromFlattenedRows({ rows, colCount })` in `selectionState` builds selection context directly from current flattened projection (`group` + `leaf` rows).
- This keeps row-index clamping and row-id resolution aligned with flattened projection, including collapsed groups.

## UI Adapter Contract

UI needs only render metadata:

```ts
interface DataGridRowRenderMeta {
  level: number
  isGroup: boolean
  isExpanded?: boolean
  hasChildren?: boolean
}
```

Canonical helper:

- `getDataGridRowRenderMeta(rowNode)` in `@affino/datagrid-core` derives adapter render meta from row-model output.

- indent is derived from `level`
- disclosure icon is derived from `isGroup` and `isExpanded`
- virtualization remains tree-agnostic
