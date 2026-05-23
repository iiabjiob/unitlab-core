# DataGrid Model Contracts

Updated: `2026-05-21`

This document defines the canonical model layer for `@affino/datagrid-core`.

## Row Model

`DataGridRowModel` is the canonical data contract for viewport/runtime layers.

Core API:

- `getSnapshot()` -> metadata (`rowCount`, `loading`, `error`, `viewportRange`, `sortModel`, `filterModel`, `groupBy`, `groupExpansion`)
- `getRow(index)` / `getRowsInRange(range)` -> row access via canonical `DataGridRowNode`
- `setViewportRange(range)` -> deterministic demand window
- `setSortModel(model)` / `setFilterModel(model)` -> model-side inputs
- `setGroupBy(spec | null)` -> grouping projection input contract
- `toggleGroup(groupKey)` -> expansion state command for grouped projection
- `refresh(reason)` -> reload hook
- `subscribe(listener)` / `dispose()` -> lifecycle

`DataGridRowNode` identity/state contract:

- `kind` (`leaf|group`) as canonical projection discriminator
- `rowKey` (stable row identity for selection/focus/operations)
- `sourceIndex` (source model index)
- `displayIndex` (current rendered/order index)
- `groupMeta` for projected group rows (`groupKey`, `groupField`, `groupValue`, `level`, `childrenCount`)
- `state.selected`
- `state.group`
- `state.pinned` (`none|top|bottom`)
- `state.expanded`
- `stickyTop/stickyBottom` are intentionally not part of core model and are mapped only at UI adapter boundaries.
- `rowKey/rowId` are mandatory. Core no longer falls back to row index for identity.

Implementations:

- `createClientRowModel` (in-memory)
- `createServerBackedRowModel` (adapter over `ServerRowModel`)
- `createDataSourceBackedRowModel` (pull+push `DataGridDataSource` protocol with abort-first cancellation/backpressure diagnostics)

Identity resolver contract:

- `createClientRowModel({ resolveRowId })` lets adapters inject deterministic identity for plain row data. Structured row-node input must use canonical `DataGridRowNode` fields.
- `createServerBackedRowModel({ resolveRowId })` is the preferred path for server datasets; default behavior reads `row.id` and throws if identity is missing.

RowModel kind truthfulness:

- Public `DataGridRowModelKind` currently includes only implemented kinds: `client | server`.
- `infinite` / `viewport` kinds are intentionally not exposed until concrete implementations and contract tests are added.

## GroupBy Projection Contract

Canonical GroupBy direction:

- GroupBy belongs to RowModel as projection logic, not to UI-only state.
- Group rows are virtual rows in the same flattened stream consumed by virtualization.
- Grid runtime remains flat-window based; tree shape exists in projected row nodes.

Implemented baseline:

- `setGroupBy(spec | null)` where `null` resets to plain grid.
- `toggleGroup(groupKey)` with stable string identity.
- explicit row kinds (`group` / `leaf`) in model output.

Projection order (must stay deterministic):

- filter -> sort -> groupBy -> flattenTree(expansionState) -> virtualization.

Execution notes:

- `createClientRowModel` executes this projection order locally and exposes flattened rows as visible projection.
- server/data-source-backed models propagate `sortModel`, `filterModel`, `groupBy`, `groupExpansion` through protocol state and keep viewport consumption on flattened row stream.
- selection adapters must resolve row indexes/ids against flattened projection; `selectionState.createGridSelectionContextFromFlattenedRows` is the canonical helper for this boundary.
- adapters derive tree-like render hints through `getDataGridRowRenderMeta(rowNode)`; viewport/virtualization stay tree-agnostic.

Reference: `docs/datagrid-groupby-rowmodel-projection.md`.

## Sort Comparator And Aggregation Extension Contract

Sort descriptors may include an optional serializable `comparator` policy:

- `default`: current deterministic value comparison.
- `locale`: `Intl.Collator`-based text comparison with optional `locale`, `numeric`, `sensitivity`, `caseFirst`, and `nulls`.
- `natural`: locale comparison with numeric ordering and base sensitivity.
- `custom`: named comparator resolved from `createClientRowModel({ comparatorRegistry })`.

Custom comparator functions are intentionally not stored in `sortModel`; use `comparatorId` so snapshots and server/data-source requests stay serializable. Client sorting, grouped projection order, pivot row sorting, worker fallback execution, and data-source pull serialization all preserve the same descriptor shape.

Aggregation extensions use `createClientRowModel({ aggregationRegistry })` plus `op: "custom"` and `aggregationId` on aggregation columns or pivot values. Registry entries provide `createState`, `add`, optional `merge`/`remove`, optional `finalize`, and optional `coerce`. Invalid registry entries fail during model creation; missing `aggregationId` references are skipped instead of silently running an incomplete custom aggregate.

Built-in aggregation ops remain unchanged. Custom registry aggregation currently uses the non-incremental aggregation path unless it is implemented as a built-in incremental op.

## TreeView Contract (DataGrid Baseline)

TreeView in DataGrid is treated as **row projection**, not UI state:

- hierarchy is produced as virtual group rows (`kind="group"`) + leaf rows (`kind="leaf"`)
- expansion/collapse is model state (`groupExpansion`) via `toggleGroup(groupKey)`
- viewport consumes flattened rows only (tree-agnostic virtualization)
- adapters derive render hints from row meta (`level`, `isGroup`, `isExpanded`, `hasChildren`)

Current baseline implementation uses `groupBy` projection in `DataGridRowModel`.

Detailed treeData contract, policies, and migration examples:

- `docs/datagrid-tree-data.md`

## Column Model

`DataGridColumnModel` is the canonical ownership boundary for column state.

Headless contracts:

- `DataGridColumnDef` = immutable definition only:
  - `key`, `field`, `label`, `columnGroup`, `minWidth`, `maxWidth`, `dataType`, `presentation`, `capabilities`, `constraints`, `cellInteraction`, `meta`
- `DataGridColumnInput` = authored input:
  - `DataGridColumnDef` + optional `initialState`
- `DataGridColumnState` = runtime mutable state:
  - `visible`, `pin`, `width`
- UI-only fields (`isSystem`, `sticky*`, legacy pin fields) are not part of the core contract
- adapter-specific payload is allowed only through `meta` (opaque boundary channel)
- `columnGroup` accepts a group id, `{ id, label }`, or a nested path; snapshots expose the normalized immutable `groupPath`
- group headers are derived from visible columns, so reorder, pin, resize, and visibility changes update group span/width without extra persisted state

Authored state must be nested under `initialState`:

```ts
const columns = [
  {
    key: "amount",
    label: "Amount",
    dataType: "currency",
    initialState: { width: 140, pin: "right" },
    presentation: {
      align: "right",
      headerAlign: "right",
      numberFormat: {
        locale: "en-GB",
        style: "currency",
        currency: "GBP",
        minimumFractionDigits: 2,
        maximumFractionDigits: 2,
      },
    },
    capabilities: { sortable: true, filterable: true, aggregatable: true },
    constraints: { min: 0 },
  },
  {
    key: "start",
    label: "Start",
    dataType: "date",
    initialState: { width: 140 },
    presentation: {
      dateTimeFormat: {
        locale: "en-GB",
        year: "numeric",
        month: "short",
        day: "2-digit",
      },
    },
  },
]
```

`presentation.numberFormat` and `presentation.dateTimeFormat` affect display formatting only. Raw cell values remain unchanged for editing, clipboard and data patches. The shared headless implementation lives in `@affino/datagrid-format`.

Interactive cell contract:

- `cellInteraction` is the canonical column-level contract for keyboard-accessible in-cell actions that still participate in the grid-owned focus model.
- `cellInteraction` is additive to `cellRenderer` and does not replace the grid shell's ownership of focus, selection, clipboard, fill, menus, or editing.
- supported invocation channels are `click`, `Enter`, and `Space`; keyboard triggers are declared per column.
- interaction accessibility semantics are column-owned through `role`, `label`, `pressed`, and `checked`.
- runtime interaction context includes `row`, `rowId`, `value`, `editable`, and the resolved `column` definition.

Example:

```ts
const columns = [
  {
    key: "approval",
    label: "Approval",
    capabilities: { editable: false },
    cellInteraction: {
      click: true,
      keyboard: ["enter", "space"],
      role: "button",
      label: ({ row }) => row?.approved ? "Reopen approval" : "Approve row",
      pressed: ({ row }) => row?.approved === true,
      onInvoke: ({ rowId, trigger }) => {
        console.log("invoke approval", rowId, trigger)
      },
    },
  },
]
```

Normalization guarantees:

- `cellInteraction.keyboard` is normalized to a frozen array.
- the normalized column snapshot preserves the authored `cellInteraction` object as immutable runtime metadata.
- runtime keyboard and click action resolution uses `cellInteraction` before built-in cell-type defaults.

Core API:

- `getSnapshot()` / `getColumn(key)`
- `setColumns(columns)`
- `setColumnOrder(keys)`
- `setColumnZoneOrder(zone, keys)`
- `setColumnVisibility(key, visible)`
- `setColumnWidth(key, width)`
- `setColumnPin(key, pin)`
- `subscribe(listener)` / `dispose()`

Snapshot guarantees:

- deterministic order output
- `order` is the stable base/global order; pinning must not mutate it
- `zoneOrder.pinnedLeft`, `zoneOrder.center`, and `zoneOrder.pinnedRight` are the projected layout order
- `visibleColumns` is projected as pinned-left, center, pinned-right
- `byKey`, `pinnedLeftColumns`, `centerColumns`, `pinnedRightColumns` are derived snapshot indexes/segments
- saved column state may include `zoneOrder` to restore pinned-zone order independently from base order
- snapshots are structurally frozen

Dynamic visibility contract:

- `setColumnVisibility(key, visible)` is the canonical runtime command
- `visibleColumns` in snapshot is the source of truth for viewport/render adapters
- selection/clipboard/navigation must resolve column indices against visible projection, not raw schema order

Viewport boundary:

- `dataGridViewportController` consumes `columnModel` as the only column source.
- Adapter (`useTableViewport`) owns normalization from incoming column props into `DataGridColumnModel`:
  legacy pin/sticky fields -> canonical `pin`, and UI-specific fields -> `column.meta`.

## Edit Model

`DataGridEditModel` is the canonical headless editing state service.

Core API:

- `getSnapshot()` -> deterministic snapshot (`revision`, sorted `edits`)
- `getEdit(rowId, columnKey)` -> single-cell patch lookup
- `setEdit(patch)` / `setEdits(patches)` -> deterministic write path
- `clearEdit(rowId, columnKey)` / `clearAll()`
- `subscribe(listener)` / `dispose()`

Determinism guarantees:

- no DOM/framework dependencies
- stable snapshot ordering by `rowId` + `columnKey`
- repeated writes with unchanged payload are no-op

## Transaction Service

`createDataGridTransactionService` defines headless command orchestration with rollback safety (advanced entrypoint).

Core contract:

- `applyTransaction({ commands })` -> atomic apply or fail with rollback
- `beginBatch` / `commitBatch` / `rollbackBatch` -> deterministic batching
- `undo` / `redo` -> deterministic history over committed transactions
- `getSnapshot()` -> deterministic state (`revision`, `pendingBatch`, `undoDepth`, `redoDepth`)

Command invariants:

- each command must include `type`, `payload`, and `rollbackPayload`
- intent metadata is first-class: `meta.intent` + `meta.affectedRange` on transaction/command
- rollback always uses `rollbackPayload` and runs in reverse command order
- same command sequence gives same apply/undo/redo ordering
- history is intent-level (transaction/batch), not per-cell event stream
- optional `maxHistoryDepth` caps undo memory growth without changing apply semantics

## Advanced Filter Contract

`DataGridFilterSnapshot` supports both compatibility and canonical expression input:

- canonical: `advancedExpression` AST (`condition | group | not`)

Expression rules:

- condition: `{ key, operator, value, value2?, type? }`
- group: `{ operator: "and" | "or", children[] }`
- not: `{ child }`

Projection order remains unchanged:

- filter (including advanced expression) -> sort -> group/tree projection -> flatten -> virtualization

Compatibility guarantee:

- `advancedExpression` is the canonical advanced filter transport shape.

## Selection Summary Contract

`createDataGridSelectionSummary` is a headless aggregation engine over current selection scope.

Input boundary:

- selection snapshot (ranges/active cell)
- row source (`rowCount`, `getRow`)
- visible column resolver (`getColumnKeyByIndex`)

Output:

- immutable summary snapshot (`selectedCells`, `selectedRows`, per-column metrics)
- supported metrics: `count`, `countDistinct`, `sum`, `avg`, `min`, `max`

API bridge:

- `DataGridApi.summarizeSelection(...)` computes summary directly from core services (rowModel + columnModel + selection).

## Public API Tiers

- Stable (`@affino/datagrid-core`): row/column/edit model contracts + core/grid API.
- Advanced (`@affino/datagrid-core/advanced`): transaction service and data-source-backed row model.
- Internal (`@affino/datagrid-core/internal`): unsafe model helpers (no semver guarantees).

## Data Source Protocol

`DataGridDataSource` is the canonical server data boundary for row-demand driven models.

Core contracts:

- `pull(request)` with `range`, `priority`, `reason`, `AbortSignal`, `sortModel`, `filterModel`
- optional `subscribe(listener)` for push events (`upsert`, `remove`, `invalidate`)
- optional `invalidate(invalidation)` for partial/full cache invalidation handoff

Reference: `docs/datagrid-data-source-protocol.md`.
