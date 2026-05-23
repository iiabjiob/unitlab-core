# DataGrid Sheets Baseline: User Interactions + Integrator API

Updated: `2026-05-17`
Scope: `/datagrid` demo baseline and `@affino/datagrid-core` integration contract.

## 1) End-User Interactions (Behavior Contract)

### Selection and navigation

- Click cell: sets active cell and single-cell selection.
- `Ctrl/Cmd + click`: appends an independent committed cell/range selection instead of replacing the current selection.
- `Shift + click` / `Shift + arrows`: extends range from fixed anchor.
- Click column header: selects the full visible column; `Shift` extends from the active column and `Ctrl/Cmd` adds another column range.
- Drag on cells: expands range continuously; auto-scroll on viewport edges (X/Y).
- `Tab` / `Shift+Tab`: horizontal navigation over navigable columns.
- `Home` / `End` and `Ctrl/Cmd + Home/End`: row/dataset edge jumps.
- `PageUp` / `PageDown`: viewport-sized vertical steps.
- `Escape`: clears active range selection.

### Edit, fill, move

- Double-click editable cell: inline edit mode.
- Enum-like cells: open value picker from in-cell trigger.
- `Enter`: commit edit; `Escape`: cancel; `Tab`: commit and move.
- Fill handle: drag from range-end handle to extend the fill range (editable columns only; non-editable cells are skipped).
- Double-click fill handle: apply the current selection down to the last row in the active projection.
- Post-fill action menu: after fill commit, the user can switch the last fill between `Series` and `Copy`; the change reapplies to the whole last fill range, while the menu stays pinned inside the visible viewport area.
- Default fill behavior: numeric source matrices default to `Series`; non-numeric values default to `Copy`.
- Move range: drag selection border to move values (editable columns only; non-editable cells are blocked).

### Clipboard and context menu

- `Ctrl/Cmd + C`: copy selected range.
- Copied-range outline is retained for each committed selection range, not only the active block.
- `Ctrl/Cmd + V`: paste at active target (matrix-aware).
- `Ctrl/Cmd + X`: cut (copy + clear editable cells).
- Context menu (`Shift+F10` or mouse right click): copy/paste/cut/clear and header actions (sort/filter/auto-size).
- The detailed mounted app clipboard contract lives in `docs/datagrid-clipboard.md`.

### History (Undo/Redo)

- `Ctrl/Cmd + Z`: undo last committed intent transaction.
- `Ctrl/Cmd + Shift + Z` (and `Ctrl + Y`): redo.
- Toolbar controls `Undo` / `Redo` map to the same transaction history.
- History entries are intent-level (`edit`, `paste`, `cut`, `clear`, `fill`, `move`) with affected range metadata.

## 2) Interaction Ownership Boundary

The canonical Vue UI path is intentionally split across the app package, Vue composables, shared orchestration utilities, and core services. Keep new interaction behavior inside the existing owner unless a public API proposal explicitly changes the boundary.

| Interaction area | Primary owner | Boundary rule |
| --- | --- | --- |
| Body scroll and viewport sampling | `packages/datagrid-vue-app/src/stage/useDataGridStageViewportRuntime.ts` and `packages/datagrid-vue/src/app/useDataGridAppViewport.ts` | Body viewport remains the native scroll surface; linked panes/header route into the body viewport and must not become competing scroll owners. |
| Core viewport math and IO | `packages/datagrid-core/src/viewport/*` | Core owns deterministic viewport math, scroll IO services, virtualization ranges, and sync contracts; Vue code consumes these contracts instead of duplicating math. |
| Cell selection and drag selection | `packages/datagrid-vue/src/app/useDataGridAppCellSelection.ts` and `packages/datagrid-vue/src/app/useDataGridAppInteractionController.ts` | The app interaction controller starts and previews pointer selection; selection snapshots are applied through the app selection adapter/runtime API. |
| Fill handle and fill lifecycle | `packages/datagrid-orchestration/src/fill/*` with app wiring in `useDataGridAppInteractionController.ts` | Orchestration owns start/stop lifecycle semantics; the app controller owns row/materialization checks, fill commit, history, and server fill handoff. |
| Range move | `packages/datagrid-orchestration/src/selection/*` with app wiring in `useDataGridAppInteractionController.ts` | Orchestration owns preview lifecycle; the app controller owns eligibility, commit behavior, selection restoration, and server-backed blocking. |
| Column resize | `packages/datagrid-orchestration/src/headers/useDataGridHeaderResizeOrchestration.ts` and `packages/datagrid-vue/src/app/useDataGridAppHeaderResize.ts` | Header resize stops competing fill/drag selection before taking owner; stage scroll sync only forwards pointer movement while resize is active. |
| Row resize | `packages/datagrid-vue/src/app/useDataGridAppRowSizing.ts` | Row sizing owns row-resize state, row-height overrides, and resize listeners; it must remain visible in active-owner diagnostics. |
| Keyboard commands | `packages/datagrid-orchestration/src/navigation/useDataGridKeyboardCommandRouter.ts` | The command router owns copy/paste/cut/clear, undo/redo, select all, context menu, and range-move cancel routing; cell navigation/edit starts remain app-controller responsibilities. |
| Focus restoration | `packages/datagrid-vue-app/src/stage/useDataGridStageFocusRuntime.ts`, `packages/datagrid-vue/src/app/dataGridFocusRestore.ts`, and orchestration focus helpers | Focus changes should use `preventScroll` when preserving viewport position matters and must not steal focus during active scroll, active editor, fill, or range-move ownership. |
| Inline editing lifecycle | `packages/datagrid-vue/src/app/useDataGridAppInteractionController.ts` and stage cell-rendering/editor handlers | Active inline editors commit before selected-cell range move or fill-handle drag claims the gesture. Scroll-active touch/desktop edit starts remain suppressed at the stage boundary. |
| Context menu | `packages/datagrid-orchestration/src/contextMenu/useDataGridViewportContextMenuRouter.ts` and stage context-menu handlers | Context menu routing may adjust selection before opening, but it must not open or mutate selection during active drag/fill/range/resize interactions. |
| Inline editing | `packages/datagrid-vue-app/src/stage/useDataGridTableStageCellIo.ts`, `packages/datagrid-vue/src/app/useDataGridAppInlineEditing.ts`, and `packages/datagrid-orchestration/src/editing/useDataGridInlineEditorFocus.ts` | Editing owns draft/commit/cancel while active; pointer selection commits the previous editor before selecting a new cell. |

Active interaction diagnostics use `packages/datagrid-vue/src/app/dataGridInteractionOwner.ts` as the current owner snapshot contract for drag selection, fill, range move, column resize, and row resize. The snapshot is internal diagnostic state, not a public integrator API.

### Selection state machine

Selection state is a coordinated app-stage contract, not a single object owned by one package. Core owns the pure selection snapshot shape and geometry; the Vue app layer owns state transitions and operation eligibility; the mounted stage owns DOM focus, rendered affordances, overlays, and editor surfaces.

| State area | Canonical owner | Transition rule |
| --- | --- | --- |
| Cell ranges and active range index | `packages/datagrid-core/src/selection/snapshot.ts` through app selection wiring | Selection mutations replace, extend, add, or clear ranges through normalized snapshot helpers. The active range is the range used for active borders, fill handle, range move, clipboard target, and keyboard extension unless a specific operation documents otherwise. |
| Active cell | Core snapshot shape, applied by `packages/datagrid-vue/src/app/useDataGridAppSelection.ts` | Click, keyboard navigation, drag start, paste target, and edit target update active cell with the committed selection snapshot. A missing or stale active cell must not be inferred from DOM focus alone. |
| Selection anchor | `packages/datagrid-vue/src/app/useDataGridAppSelection.ts` and `useDataGridAppCellSelection.ts` | Shift extension uses the app anchor. Replacing the selection resets the anchor; additive selection commits a separate range and preserves the active range according to the app selection transition. |
| Row selection | `packages/datagrid-core/src/selection/rowSelection.ts`, `packages/datagrid-vue/src/app/useDataGridAppRowSelection.ts`, and stage row-selection UI | Row selection is separate from cell-range selection. Checkbox and row-index interactions update row-selection state and focused row; they must not silently rewrite cell ranges unless the interaction contract explicitly says it selects a row range. |
| DOM focus | `packages/datagrid-vue-app/src/stage/useDataGridStageFocusRuntime.ts` and `packages/datagrid-vue/src/app/dataGridFocusRestore.ts` | Focus follows the active cell when a rendered focus target exists. Restoration uses `preventScroll` when viewport position must be preserved and must not steal focus from an active editor, fill gesture, range move, resize, context menu, or native scroll. |
| Inline editing | `packages/datagrid-vue/src/app/useDataGridAppInteractionController.ts` and stage editor handlers | Editing owns draft, commit, cancel, and editor focus while active. Pointer selection, fill, and range move commit or cancel the previous editor before they claim the next interaction. |
| Pending clipboard ranges | `packages/datagrid-vue/src/app/useDataGridAppClipboard.ts` | Copy/cut ranges are retained as committed selection metadata for local materialized rows. Unloaded, placeholder, or stale virtual ranges must be blocked or delegated by the server-backed operation contract. |
| Fill preview | `packages/datagrid-vue/src/app/useDataGridAppInteractionController.ts` with `packages/datagrid-orchestration/src/fill/*` | Fill preview is a transient interaction state owned by the active fill gesture. It is cleared on commit, cancel, projection invalidation, or competing owner start. |
| Range-move preview | `packages/datagrid-vue/src/app/useDataGridAppInteractionController.ts` with `packages/datagrid-orchestration/src/selection/*` | Range move preview is a transient interaction state owned by the active range-move gesture. It is cleared on commit, cancel, projection invalidation, or competing owner start. |

State transitions follow these rules:

- A committed selection snapshot is the logical source of truth. Rendered classes, overlays, handles, and copied-range outlines are materialized views of that snapshot plus transient interaction previews.
- DOM focus is never the source of truth for selected ranges. It may lag while the active cell is virtualized out, hidden by horizontal virtualization, or represented by a placeholder.
- Active editing temporarily owns keyboard/text focus. Selection restoration waits until editing commits or cancels unless the edit transition explicitly hands control back to the grid.
- Projection changes (`sort`, `filter`, `group`, `pivot`, tree expansion, and datasource cache replacement) must preserve, clear, rebase, or mark selection state stale through an explicit invalidation policy. Stale virtual selections must not run local materialized operations.
- Cell-range selection and row selection remain separate state machines. Shared keyboard, focus, and context-menu paths must choose a single target state before mutating either one.
- Touch selection remains scroll-first until a documented mode transition or explicit touch affordance claims selection ownership.

### Multi-range visual contract

- Additive cell ranges remain part of the committed selection snapshot and render selected-cell highlighting for every selected rendered cell.
- The active range owns active affordances: bordered overlay lanes, pinned-pane seam overlays, fill handle placement, range-move edge hover, clipboard target, and keyboard extension.
- Inactive additive ranges do not render active overlay borders or edge hover affordances. They remain visible through cell selected styling and available to selection-aware operations that explicitly consume all ranges.
- Pinned left, center, right, and pinned-bottom overlay lanes render the active range only when additive selection is active. Non-additive single-range selection renders its one committed range in every intersecting pane.

### Selection accessibility contract

- Rendered body cells expose `aria-selected` from the logical selection snapshot: anchor cells and selected cells are `true`, unselected rendered cells are `false`.
- Multi-range selections expose selected state for each rendered committed range even when only the active range owns overlays, fill handle, and range-move affordances.
- Row-selection cells keep checkbox semantics separate from cell-range selection through `role="checkbox"` and `aria-checked`.
- Non-materializable placeholder cells expose `aria-disabled="true"`; materializable placeholder cells remain interactive and materialize through the same edit/toggle paths as pointer and keyboard workflows.
- Virtualized remounts must recompute accessible selected/disabled state from snapshot and row surface state after the cell returns to the DOM.

### Server-backed selection operations

Server-backed grids use the operation matrix in `docs/server-datasource/selection-operations.md`.

- Loaded data rows may use local materialized copy, cut, clear/delete, paste, fill, range move, and summary paths when the cells are editable and not group rows.
- Unloaded or placeholder rows must use a server-delegated operation when that capability exists; otherwise the operation is blocked with a clear user-facing state.
- Stale virtual selections must not run local materialized operations. The user must refresh/reselect or the app must delegate to a backend operation that validates `baseRevision` and projection identity.
- Row selection `all` mode represents all rows in the current projection with exclusions; server-backed workflows must not enumerate unloaded row ids just to represent all-row selection.

### Event policy

Mouse, touch-generated mouse, touch, wheel, keyboard, and context-menu events follow an explicit cancellation policy. The default rule is that native body scrolling and editor/input behavior win unless an affordance-owned grid interaction has already claimed the gesture.

| Event path | Owner | `preventDefault()` / passive rule |
| --- | --- | --- |
| Body viewport `scroll` | Stage viewport runtime | Passive/native. Handlers sample offsets and schedule viewport sync; they do not cancel native scroll. |
| Body cell touch-generated `mousedown` / `click` | Stage guards and app mouse guards | Touch-generated body-cell mouse events in touch/auto mode prioritize native scroll and do not start desktop drag/fill/range/resize paths. |
| Desktop cell `mousedown` | App interaction controller | May prevent default after the grid claims selection/range-move ownership. |
| Touch selection handle start | Stage touch selection handle bridge and app interaction controller | Touch range extension is allowed only from the explicit selection handle; the handle isolates/cancels its touch gesture and forwards selection extension through the existing interaction lifecycle. |
| Fill handle mouse/touch start | Stage pointer interactions and fill lifecycle | Desktop mouse may prevent default on the explicit handle. Touch fill is allowed only from the explicit handle and isolates/cancels that handle gesture. |
| Range move | App interaction controller and range-move lifecycle | Desktop selected-cell body range move is movement-threshold gated. Touch range move must use explicit touch affordances, not body-cell drag. |
| Column/row resize handles | Header resize orchestration and row sizing | Resize handles may prevent default after ownership is accepted; touch-generated desktop mouse fallback is ignored unless routed by an explicit touch affordance. |
| Linked header/pinned touch pan | `installDataGridTouchPanGuard()` | `touchstart`, `touchend`, and `touchcancel` stay passive. The non-passive `touchmove` listener is installed only after a handled linked-surface touch start and removed when the gesture ends. |
| Header wheel / linked wheel | Stage scroll sync | May prevent default only when translating the linked wheel gesture into body viewport scroll. |
| Keyboard commands | Keyboard command router | Prevents default for handled grid commands such as copy/paste/cut, clear, undo/redo, select all, context menu, and navigation. |
| Context menu | Context-menu router and mounted lifecycle cleanup | Opens grid context menu only when no active drag/fill/range/resize owner is running. Active interactions block the menu and finalize/cancel through lifecycle cleanup. |
| Inline editor input | Editor components and app editing | Editor-owned inputs keep native text-editing behavior; pointer selection commits/cancels editor state before the grid claims a new cell interaction. |

## 3) Integrator API Usage (Core)

Use stable core API from package root and advanced transaction service from advanced entrypoint.

```ts
import {
  createClientRowModel,
  createDataGridApi,
  createDataGridColumnModel,
  createDataGridCore,
} from "@affino/datagrid-core"
import { createDataGridTransactionService } from "@affino/datagrid-core/advanced"

const rowModel = createClientRowModel({ rows })
const columnModel = createDataGridColumnModel({ columns })

const transaction = createDataGridTransactionService({
  maxHistoryDepth: 120,
  execute(command, context) {
    // apply = no-op for already-applied state snapshots in UI-driven flow
    // undo/redo/rollback = restore rollback payload snapshot
  },
})

const core = createDataGridCore({
  services: {
    rowModel: { name: "rowModel", model: rowModel },
    columnModel: { name: "columnModel", model: columnModel },
    transaction: {
      name: "transaction",
      getTransactionSnapshot: transaction.getSnapshot,
      beginTransactionBatch: transaction.beginBatch,
      commitTransactionBatch: transaction.commitBatch,
      rollbackTransactionBatch: transaction.rollbackBatch,
      applyTransaction: transaction.applyTransaction,
      canUndoTransaction: transaction.canUndo,
      canRedoTransaction: transaction.canRedo,
      undoTransaction: transaction.undo,
      redoTransaction: transaction.redo,
      dispose() {
        transaction.dispose()
      },
    },
  },
})

const api = createDataGridApi({ core })
await api.start()
```

### Recommended Vue app integration

For the canonical Vue UI path, use `DataGrid` from `@affino/datagrid-vue-app`.
No extra prop is required to enable the fill handle in base table mode.

```vue
<script setup lang="ts">
import { DataGrid } from "@affino/datagrid-vue-app"

const rows = [
  { id: 1, sku: "A-100", month: 1, amount: 120 },
  { id: 2, sku: "A-100", month: 2, amount: 150 },
]

const columns = [
  { key: "sku", label: "SKU", capabilities: { editable: false } },
  { key: "month", label: "Month" },
  { key: "amount", label: "Amount" },
]
</script>

<template>
  <DataGrid
    :rows="rows"
    :columns="columns"
    :client-row-model-options="{ resolveRowId: row => row.id }"
  />
</template>
```

Notes:

- Fill handle is surfaced only in base table mode.
- Set `capabilities.editable = false` on columns that must stay read-only.
- The built-in UI supports drag-fill, double-click fill-down, and post-fill `Series` / `Copy` reapply.
- Pass `readFilterCell` when filter menus or histograms must reflect effective formula/display values instead of raw row fields.
- Pass `readSelectionCell` when aggregate labels or `api.selection.summarize(...)` should use effective values.
- Selection aggregate labels and `api.selection.summarize(...)` are local/materialized summaries. Virtual selections report loaded/local coverage only; unloaded rows require a server summary operation from the datasource contract.
- Large local summaries are budgeted to avoid cell-by-cell work over unbounded selections. When the app label reaches the local budget, it reports the full selected count plus the loaded cells included in the local aggregate and adds `budgeted`.

### Custom Vue renderer path

If you are composing your own renderer, use the app-layer hooks exported from `@affino/datagrid-vue`:

- `useDataGridAppSelection`
- `useDataGridAppClipboard`
- `useDataGridAppFill`
- `useDataGridAppInteractionController`

Do not use the removed `useAffinoDataGrid*` wrappers.

### Required integration rules

- Required services: `rowModel`, `columnModel`.
- Optional capability services: `transaction`, `selection`, `viewport`.
- Keep row identity stable (`rowId`/`rowKey`), never index-based fallback.
- Keep GroupBy in row-model pipeline (`filter -> sort -> groupBy -> flatten -> visible`).
- Treat transaction history as model-level capability, not UI-only state.
- Prefer declarative `advancedExpression` in filter snapshot for complex conditions (`and`/`or`/`not`).

### Common API operations

- Data projection:
  - `api.rows.setSortModel(...)`
  - `api.rows.setFilterModel(...)`
  - `api.rows.setGroupBy(...)`
  - `api.rows.toggleGroup(groupKey)`
- Column state:
  - `api.columns.setWidth(key, width)`
  - `api.columns.setPin(key, "left" | "right" | "none")`
  - `api.columns.setVisibility(key, visible)`
- History:
  - `api.transaction.apply(tx)`
  - `api.transaction.undo()`
  - `api.transaction.redo()`
- Selection summary:
  - `api.selection.summarize({ columns, defaultAggregations, readSelectionCell })`

## 4) Related References

- `docs/datagrid-grid-api.md`
- `docs/datagrid-model-contracts.md`
- `docs/datagrid-groupby-rowmodel-projection.md`
- `docs/datagrid-architecture.md`
- `docs/audits/INTERACTION_ORCHESTRATION_AUDIT.md`
- `e2e/sandbox-interactions.spec.ts`
