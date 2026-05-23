# DataGrid Unified Grid API

Updated: `2026-05-20`

`DataGridApi` is the semver-safe, namespace-based facade for model/service operations in `@affino/datagrid-core`.

## Entry point

Use only package public exports:

- `createDataGridApi`
- `DataGridApi`

Deep imports are outside the stable public contract.

## Namespaced surface

Stable domains:

- `api.lifecycle`
- `api.rows.*`
- `api.data.*`
- `api.columns.*`
- `api.view.*`
- `api.pivot.*`
- `api.selection.*`
- `api.transaction.*`
- `api.compute.*`
- `api.diagnostics.*`
- `api.meta.*`
- `api.policy.*`
- `api.plugins.*`
- `api.state.*`
- `api.events.*`

Lifecycle:

- `api.init()`
- `api.start()`
- `api.stop()`
- `api.dispose()`

Flat API methods are removed from `DataGridApi`.

## Capability contract

`api.capabilities` is runtime-resolved:

- `patch`
- `dataMutation`
- `backpressureControl`
- `compute`
- `selection`
- `transaction`
- `histogram`
- `sortFilterBatch`
- `viewportPosition`

Use it as guard before capability-dependent mutating calls.

## Key semantics

- `rows.getProjectedRows()` returns a new array of current projected leaf row data in `api.rows` order.
- `rowSelection.getSelectedRowData()` resolves selected row ids against current projected rows, preserving projected order and supporting all/excluded selection mode.
- `selection.getRangeRowData()` resolves material cell ranges to de-duplicated projected leaf row data in projected row order; single-cell focus ranges return an empty array.
- `rows.applyEdits(...)` mutates data (optionally with reapply policy).
- `rows.batch(...)` is an explicit bulk mutation boundary with one coalesced facade event-cycle.
- `view.reapply()` recomputes projection only.
- `view.getViewportPosition()/setViewportPosition(...)` expose semantic viewport state when the adapter provides the viewport capability.
- `pivot` remains a separate analytical subsystem (intentionally not nested under `rows`).
- `data.pause()/resume()/flush()` is the public backpressure control surface for supported server/data-source row models.
- `state.get/set` is the unified state boundary for export/import (V1 model-centric payload).
- `state.migrate(...)` is the explicit payload migration/validation hook before restore.
- `events.on` is the typed public event surface with documented in-process ordering. See [Event matrix](./datagrid-event-matrix.md) for cross-layer mapping.
- `events.on("row-selection:changed", ...)` is the typed row-selection event surface for `selectedRows` / `focusedRow` snapshots.
- `events` includes explicit state import boundaries (`state:import:begin/end`).
- `compute.switchMode(...)` is synchronous and does not implicitly trigger recompute.
- `diagnostics.getAll()` is read-only and does not trigger recompute.
- `meta.getApiVersion()/getProtocolVersion()` expose compatibility versions for multi-runtime integrations.
- `plugins` lifecycle is event-driven (`onRegister`/`onDispose`/`onEvent`).

## Runtime guarantees

- Snapshot isolation: public read methods are revision-consistent within the same synchronous call stack.
- Guarded mutation serialization: high-impact guarded operations are serialized through lifecycle exclusivity.
- Event reentrancy: reentrant emissions are queued FIFO; mutation from handlers is allowed.
- State import boundary: `state.set(...)` is a begin/end logical boundary, not single-event atomic payload.

## Concurrency and error model

- `lifecycle.runExclusive` provides exclusive mutation windows for guarded operations.
- `lifecycle.whenIdle` resolves after the exclusive queue drains.
- `lifecycle.isBusy()` reports whether guarded mutation queue / lifecycle transition is in progress.
- `events.error` provides typed recoverable runtime conflict payloads.
- Guarded operations may still throw/reject for control-flow correctness.

## Plugin safety model

- `api.plugins` is the stable public plugin facade.
- Plugins are observational by default and consume only public event payloads.
- Plugin `onRegister`, `onEvent`, and `onDispose` failures are isolated from core registry and event-dispatch paths.
- Plugins can mutate state only through public API calls.
- Advanced capability-gated plugin hosts should use `@affino/datagrid-plugins` as described in `docs/datagrid-plugin-lifecycle.md`.

## Service binding notes

`createDataGridApi` binds to `GridCore` services:

- required: `rowModel`, `columnModel`
- optional capabilities: selection, transaction, viewport, histogram, compute mode switching, data mutation support, backpressure controls

Creation is fail-fast for missing required services.

## Selection summary contract

`api.selection.summarize(options?)` computes deterministic aggregates over selected scope:

- `count`, `countDistinct`, `sum`, `avg`, `min`, `max`

Selection stays headless in core; adapter/UI mapping remains at adapter boundary.

## Row height contract (`api.view`)

Row-height semantics are part of Core `view` namespace and are adapter-consumable:

- `api.view.setRowHeightMode("fixed" | "auto")`
- `api.view.setBaseRowHeight(height)`
- `api.view.measureRowHeight()`
- `api.view.setRowHeightOverride(rowIndex, height | null)`
- `api.view.getRowHeightOverride(rowIndex)`
- `api.view.clearRowHeightOverrides()`

Semantics:

- Per-row resize/autosize persistence is owned by Core `view` state, not sandbox/component-local maps.
- `rowIndex` addresses displayed row index in active row model projection.
- Passing `null` to `setRowHeightOverride` removes the override for that row.

## Persisting sort, selection, and viewport position

Use unified state as the persistence boundary for saved views and page reload restore. By default, `api.state.get()` includes model, column layout, and selection state. Viewport position is opt-in because it depends on an adapter viewport capability.

```ts
const savedState = api.state.get({ includeViewportPosition: true })

localStorage.setItem("orders-grid-state", JSON.stringify(savedState))
```

Restore after rows, columns, and the adapter viewport are ready:

```ts
const rawState = localStorage.getItem("orders-grid-state")
const migratedState = rawState ? api.state.migrate(JSON.parse(rawState)) : null

if (migratedState) {
  api.state.set(migratedState, {
    applyViewportPosition: true,
  })
}
```

Restore order is semantic: row projection state such as sort, filter, group, pivot, and pagination is applied first, then column layout, selection, and finally viewport position. The viewport anchor prefers `rowId` and `columnKey`; if those cannot be resolved after the new projection or column visibility is applied, it falls back to `rowIndex` and `columnIndex`, then to raw `scroll.top` and `scroll.left`.

The public viewport API is:

- `api.view.getViewportPosition()`
- `api.view.setViewportPosition(position, options?)`
- `api.view.scrollToRow(target)`
- `api.view.scrollToColumn(target)`
- `api.view.scrollToCell(target)`

DOM scroll remains an adapter detail. Consumers should persist the semantic snapshot returned by `api.state.get({ includeViewportPosition: true })` or `api.view.getViewportPosition()`, not read scroll offsets directly from rendered elements.

## Viewport integration boundary

Pinned/overlay geometry sync remains in advanced viewport controller API:

- `createDataGridViewportController(...).getIntegrationSnapshot()`
- `createDataGridViewportController(...).getViewportSyncState()`

Use these for deterministic adapter geometry integration instead of internal signal reads.

## Related docs

- `docs/datagrid-core-factories-reference.md`
- `docs/datagrid-core-advanced-reference.md`
- `docs/datagrid-state-events-compute-diagnostics.md`
- `docs/datagrid-feature-catalog.md`
