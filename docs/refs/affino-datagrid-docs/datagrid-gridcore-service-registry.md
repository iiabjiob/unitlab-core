# DataGrid GridCore Service Registry

Updated: `2026-02-07`

`GridCore` is the canonical service container for datagrid engine services:

- `event`
- `rowModel`
- `columnModel`
- `edit`
- `transaction`
- `selection`
- `viewport`

Typed service contracts:

- `DataGridCoreRowModelService` (`model?: DataGridRowModel`)
- `DataGridCoreColumnModelService` (`model?: DataGridColumnModel`)
- `DataGridCoreEditService` (`model?: DataGridEditModel`)
- `DataGridCoreTransactionService` (`applyTransaction`, `begin/commit/rollback batch`, `undo/redo`, snapshot hooks)
- `DataGridCoreSelectionService` (headless selection capability methods over `DataGridSelectionSnapshot`)
- `DataGridCoreViewportService` (viewport range capability methods)

`getService(name)` is typed by service key (`DataGridCoreServiceByName`), which is used by `GridApi`.

## Lifecycle Contract

Each service can implement:

- `init(context)`
- `start(context)`
- `stop(context)`
- `dispose(context)`

Core lifecycle:

- `core.init()` -> runs `init` in deterministic startup order
- `core.start()` -> ensures init, then runs `start` in startup order
- `core.stop()` -> runs `stop` in reverse startup order
- `core.dispose()` -> runs `stop` (if needed), then `dispose` in reverse startup order

## Startup Order

Canonical order:

1. `event`
2. `rowModel`
3. `columnModel`
4. `edit`
5. `transaction`
6. `selection`
7. `viewport`

Custom `startupOrder` can prioritize a subset; missing services are appended deterministically according to canonical order.
