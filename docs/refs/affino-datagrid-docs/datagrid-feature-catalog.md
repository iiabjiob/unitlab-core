# DataGrid Feature Catalog

Updated: 2026-03-11

This is the canonical feature inventory for Affino DataGrid.
Use it as a single decision sheet to understand whether the platform fits your product requirements.

## How to read this catalog

- Scope:
  - `Core` means implemented in `@affino/datagrid-core`.
  - `Adapter` means surfaced through framework adapters (`@affino/datagrid-vue`, `@affino/datagrid-laravel`).
  - `App` means surfaced through app-facing framework facades (`@affino/datagrid-vue-app`, `@affino/datagrid-laravel-app`).
  - `Backend` means contract exists client-side, but behavior is implemented by your server/data source.
- Runtime mode:
  - `main-thread` for simpler/smaller workloads.
  - `worker-owned` for interaction-heavy workloads where UI responsiveness is critical.
  - `server-side` when query shape and data shaping should be backend-owned.

## Capability Matrix

| Area | Capability | Scope | Runtime mode | Notes |
| --- | --- | --- | --- | --- |
| API | Stable namespace-based facade (`DataGridApi`) | Core + Adapter | all | Single public surface: `lifecycle/rows/data/columns/view/pivot/selection/transaction/state/events/meta/policy/compute/diagnostics/plugins`. |
| Row models | Client row model (`createClientRowModel`) | Core + Adapter | main-thread, worker-owned | Sorting/filtering/grouping/pagination/visible projection pipeline. |
| Row models | Worker-owned row model (`createDataGridWorkerOwnedRowModel`) | Core worker + Adapter | worker-owned | Compute/state owned by worker, main thread consumes snapshots/windows. |
| Row models | Data source backed row model (`createDataSourceBackedRowModel`) | Core + Adapter + Backend | server-side | Protocol-first pull/push/invalidation/backpressure contract for backend-driven datasets. |
| Row models | Server backed row model (`createServerBackedRowModel`) | Core + Adapter + Backend | server-side | Server-fetch oriented model with explicit lazy/block fetch flow and cached snapshots. |
| State | Unified state export/import (`api.state.get/set`) | Core + Adapter | all | V1 is model-centric transport (`rows.snapshot` + `columns` + `selection` + `transaction`) for restore/integration boundaries. |
| State | State migration hook (`api.state.migrate`) | Core + Adapter | all | Explicit payload migration/validation path before restore (`strict` mode supported). |
| State | Partial + strict restore policies | Core + Adapter | all | `applyColumns/applySelection/applyViewport/strict` restore options. |
| State | Explicit import boundaries (`state:import:begin/end`) | Core + Adapter | all | Logical begin/end envelope for `api.state.set(...)`, with `state:imported` emitted on successful apply. |
| State | Snapshot isolation contract (sync stack) | Core + Adapter | all | Read operations are revision-consistent within one synchronous call stack unless a mutation is executed in between. |
| Events | Typed public event surface (`api.events.on`) | Core + Adapter | all | Typed events for rows/columns/projection/selection/pivot/transaction/viewport/state/error. |
| Events | Event ordering guarantees | Core + Adapter | all | Row tick order is explicit: `rows:changed` -> `projection:recomputed` -> `pivot:changed` -> `viewport:changed` (when applicable). |
| Events | Deterministic reentrancy queue | Core + Adapter | all | Reentrant event emissions are queued FIFO within the current runtime tick. |
| Events | Error surface (`error` event + error codes) | Core + Adapter | all | Structured error payload (`code/operation/recoverable/error`) for integration-safe recovery handling. |
| Query | Sort model (single/multi column) | Core + Adapter | all | Deterministic sort state and projection stage integration. |
| Query | Column filters | Core + Adapter | all | Predicate-based column filtering with snapshot state. |
| Query | Advanced filter expressions | Core + Adapter | all | Structured boolean expression tree normalization/evaluation. |
| Query | Quick filter (`filterModel.quickFilter`) | Core + Adapter + App + Backend | all | Grid-wide text filtering is part of `DataGridFilterSnapshot`, runs inside the filter stage before sort/group/pivot/pagination, and stays structured-clone/serialization safe. Server-side data sources receive it through `DataGridDataSourcePullRequest.filterModel` and must apply it backend-side. See [DataGrid Quick Filter](./datagrid-quick-filter.md). |
| Grouping | Group by model + expansion state | Core + Adapter | all | Deterministic group projection with expand/collapse controls. |
| Tree data | Tree projection and subtree toggles | Core + Adapter | all | Tree data projection paths and group-like expansion controls. |
| Aggregation | Built-in aggregations (`sum/count/countNonNull/avg/min/max/first/last`) | Core + Adapter | all | Incremental paths where applicable; deterministic finalize behavior. |
| Aggregation | Custom aggregation hooks | Core + Adapter | all | Custom add/finalize/merge style aggregation contracts. |
| Pivot | Pivot rows/columns/values model | Core + Adapter | main-thread, worker-owned, server-side* | Declarative pivot spec with dynamic pivot column generation. |
| Pivot | Row subtotals and grand total | Core + Adapter | main-thread, worker-owned, server-side* | Configurable totals in pivot projection. |
| Pivot | Pivot drilldown (`getPivotCellDrilldown`) | Core + Adapter | main-thread, worker-owned, server-side* | Details path from pivot cell back to source rows. |
| Pivot | Pivot layout export/import + interop snapshot | Core + Adapter | main-thread, worker-owned, server-side* | Persist/restore pivot layout and cross-boundary interop payloads. |
| Pagination | Pagination model/snapshot | Core + Adapter | all | Deterministic paging inputs and snapshot outputs. |
| Virtualization | Vertical virtualization (rows) | Core + Adapter | all | Viewport-driven visible row windows for large datasets; see the [virtualization support matrix](./datagrid-virtualization-support-matrix.md) for enterprise limits. |
| Virtualization | Horizontal virtualization (columns) | Core + Adapter | all | Deterministic horizontal windowing with pinned column support; enabled explicitly for wide app grids. |
| Columns | Visibility, order, sizing, pinning | Core + Adapter | all | Canonical column model (`pin` contract, snapshots, state updates). |
| App UX | Built-in column menu with declarative trigger, section/action config, disabled reasons, custom items, and async value filters | App | main-thread, worker-owned, server-side* | `columnMenu` supports boolean enablement plus object-form `trigger`, `items`, `disabled`, `disabledReasons`, `labels`, `actions`, `customItems`, and per-column overrides for the standard `sort/group/pin/filter` menu. Value-filter lists can resolve from async row-model histograms and forward server-side search. |
| App UX | Localizable built-in toolbar panels | App | main-thread, worker-owned | `advancedFilter.labels` and `columnLayout.labels` cover trigger text, panel chrome, actions, field labels, join labels, and advanced-filter operator labels for app-level localization. |
| App UX | Declarative history facade (`history` prop + stable controller) | App | main-thread, worker-owned | `history` supports built-in or injected adapters, transaction-level undo depth, `grid`/`window` shortcut routing, optional built-in toolbar controls, and stable `canUndo/canRedo/runHistoryAction` access from the component ref. |
| App UX | Visual placeholder row tail with lazy materialization (`placeholderRows`) | App | main-thread, worker-owned | `rows` stay real-only while `placeholderRows.count` adds an Excel-like visual tail; `createRowAt(...)` materializes real rows on first edit/paste/toggle, built-in history can undo that materialization back to placeholder state, and the Vue sandbox base-table card exposes a live placeholder-tail demo. |
| App UX | Saved-view facade helpers + persistence adapters | App | main-thread, worker-owned | Imperative `getSavedView()` / `migrateSavedView()` / `applySavedView()` persist unified runtime state together with app `viewMode`, while `serialize/parse/read/write/clear` helpers support storage-backed presets. |
| Selection | Row and cell/range selection | Core + Adapter | all | Anchor/focus/range model and overlay transform contracts. |
| Selection | Fill handle + drag-fill | Core + Adapter + App | main-thread, worker-owned | Spreadsheet-like fill interaction primitives, including drag-fill, double-click fill-down, and post-fill `Series` / `Copy` reapply in app facades. |
| Selection | Range move / drag-move | Core + Adapter | main-thread, worker-owned | Move selected ranges with deterministic lifecycle hooks. |
| Clipboard | Copy/cut/paste orchestration | Core + Adapter | main-thread, worker-owned | Clipboard bridge/mutation policy and selection-aware operations. |
| Reordering | Client row reorder mutation (`rowModel.reorderRows`) | Core + Adapter | main-thread, worker-owned | Officially row-model-scoped in current stable facade; adapters may wrap it, `api.rows` alias is not yet part of semver contract. |
| Editing | Patch updates (`patchRows`) | Core + Adapter | all | Partial row updates with field-aware stage invalidation. |
| Editing | Editing lifecycle controls (`patch/applyEdits/reapply`) | Core + Adapter | main-thread, worker-owned | Explicit mutation lifecycle for edit-heavy UX flows. |
| Editing | Abortable mutation dispatch (`AbortSignal`) | Core + Adapter | all | `rows.patch`, `rows.applyEdits`, `transaction.apply` support abort-before-dispatch semantics. |
| Editing | Projection policy (`mutable/immutable/excel-like`) | Core + Adapter | all | `mutable` enables auto-reapply; `immutable`/`excel-like` disable auto-reapply (explicit reapply flow). |
| Editing | Deterministic edit revisions | Core + Adapter | all | Revision snapshots are monotonic across edit pipelines. |
| Editing | Stage invalidation guarantees | Core + Adapter | all | Field-aware invalidation keeps recompute scope explicit and observable. |
| Interaction | Keyboard command routing | Core + Adapter | main-thread, worker-owned | Advanced keyboard orchestration for editing/selection/navigation. |
| Interaction | Pointer/context-menu orchestration | Core + Adapter | main-thread, worker-owned | Advanced pointer routing and context menu action contracts. |
| Accessibility | A11y attributes + state machine | Core advanced + Adapter | all | Deterministic ARIA mapping and keyboard/a11y state support. |
| Compute | Mode switch (`api.compute.getMode/switchMode`) | Core + Adapter | main-thread, worker-owned | Synchronous mode switch control for sync/worker paths; does not implicitly recompute projection. |
| Lifecycle | Concurrency helpers (`api.lifecycle.isBusy/whenIdle/runExclusive`) | Core + Adapter | all | Public lifecycle guard layer for serializing high-impact operations. |
| Diagnostics | Aggregated diagnostics snapshot (`api.diagnostics.getAll`) | Core + Adapter | all | Read-only diagnostics payload (`rowModel/compute/cache/backpressure`) without triggering recompute. |
| Diagnostics | Projection stage health | Core + Adapter | all | Stage/version/stale diagnostics for runtime health and regression debugging. |
| Diagnostics | Compute transport health | Core + Adapter | main-thread, worker-owned | Dispatch/fallback/transport diagnostics for worker and hybrid pipelines. |
| Metadata | API/protocol version introspection (`api.meta.getApiVersion/getProtocolVersion`) | Core + Adapter | all | Enables compatibility checks in multi-runtime integrations. |
| Transactions | Transaction service (undo/redo/batch/rollback) | Core advanced + Adapter | main-thread, worker-owned | Advanced transaction capability surface. |
| Determinism | Deterministic projection lifecycle | Core + Adapter | all | Projection recompute lifecycle is snapshot-driven and stage-aware. |
| Determinism | Revision-based state tracking | Core + Adapter | all | Monotonic revisions support restore/checkpoint/test assertions. |
| Determinism | Predictable mutation pipeline | Core + Adapter | all | Structured mutation + event payloads support deterministic integration flows and regression assertions. |
| Extensibility | Stable plugin registration surface (`api.plugins.*`) | Core + Adapter | all | Register/unregister/list plugins on stable API facade with lifecycle hooks (`onRegister/onDispose`) and event tap (`onEvent`). |
| Extensibility | Plugin sandbox contract | Core + Adapter | all | Plugins are observational by default; state changes must go through public API and cannot mutate internal service registry directly. |
| Extensibility | Advanced runtime plugin hooks | Core advanced + Adapter | all | Host/plugin runtime hooks for power-user integration layers. |
| Protocols | Worker protocol (commands/updates/transport) | Core worker + Adapter | worker-owned | Typed worker messaging for compute and row-model ownership. |
| Protocols | Data source pull/push/invalidation/backpressure | Core + Adapter + Backend | server-side | Backend integration contract for remote and streaming data flows. |
| Quality | Contract + perf gate coverage | Repo process | all | CI gates for API contracts, perf budgets, variance and drift checks. |

\* `server-side` pivot depends on backend implementation through data source/server protocols.

## Runtime Mode Decision Table

| Requirement profile | Recommended mode |
| --- | --- |
| Small-to-medium table, low mutation pressure, simple UX | `main-thread` |
| Frequent edits/patch storms + active sort/group/filter with responsiveness constraints | `worker-owned` |
| Large remote dataset, backend-owned filtering/grouping/pivot/query | `server-side` |
| Mixed workload with uncertain profile | Start `main-thread`, benchmark, then promote to `worker-owned` or `server-side` |

## Package Entry Map

| Package | Intended consumer | Typical use |
| --- | --- | --- |
| `@affino/datagrid` | App teams | Stable application-facing DataGrid facade. |
| `@affino/datagrid-core` | Headless/platform engineers | Stable community-safe core contracts and client row-model primitives. |
| `@affino/datagrid-vue` | Vue teams | Community-safe Vue adapter surface. |
| `@affino/datagrid-vue-app` | Vue app teams | App-facing Vue facade with opinionated install path. |
| `@affino/datagrid-laravel` | Laravel/Livewire teams | Community-safe Laravel facade. |
| `@affino/datagrid-laravel-app` | Laravel app teams | App-facing Laravel facade with future opinionated DX boundary. |
| `@affino/datagrid-vue/advanced` | Power users | Low-level interaction/layout primitives for custom renderer wiring. |
