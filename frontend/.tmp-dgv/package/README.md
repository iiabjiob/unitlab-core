# @affino/datagrid-vue

Vue adapter surface for `@affino/datagrid-core`.

## Stable API (`@affino/datagrid-vue`)

- `createDataGridVueRuntime`
- `useDataGridRuntime`
- `useAffinoDataGrid`
- `useAffinoDataGridUi`
- `AffinoDataGridSimple`
- `DataGrid`
- `useDataGridSettingsStore`
- `createDataGridSettingsAdapter`
- `buildDataGridOverlayTransform`
- `buildDataGridOverlayTransformFromSnapshot`
- `mapDataGridA11yGridAttributes`
- `mapDataGridA11yCellAttributes`
- `useDataGridContextMenu`
- `DATA_GRID_SELECTORS`
- `DATA_GRID_DATA_ATTRS`
- `dataGridCellSelector`
- `dataGridHeaderCellSelector`
- `dataGridResizeHandleSelector`

Stable selector contract (for parity tests/integration-safe querying):

```ts
import {
  DATA_GRID_SELECTORS,
  dataGridCellSelector,
} from "@affino/datagrid-vue"

const viewportSelector = DATA_GRID_SELECTORS.viewport
const ownerCellSelector = dataGridCellSelector("owner")
```

## Advanced API (`@affino/datagrid-vue/advanced`)

Compatibility entrypoint. New demo/workbench wiring should import `@affino/datagrid-vue/internal`.

- `useDataGridViewportBridge`
- `useDataGridHeaderOrchestration`
- `createDataGridHeaderBindings`
- `useDataGridCellPointerDownRouter`
- `useDataGridCellPointerHoverRouter`
- `useDataGridDragSelectionLifecycle`
- `useDataGridDragPointerSelection`
- `useDataGridFillSelectionLifecycle`
- `useDataGridFillHandleStart`
- `useDataGridRangeMoveLifecycle`
- `useDataGridRangeMoveStart`
- `useDataGridSelectionMoveHandle`
- `useDataGridTabTargetResolver`
- `useDataGridCellNavigation`
- `useDataGridClipboardValuePolicy`
- `useDataGridCellDatasetResolver`
- `useDataGridCellRangeHelpers`
- `useDataGridNavigationPrimitives`
- `useDataGridMutationSnapshot`
- `useDataGridCellVisualStatePredicates`
- `useDataGridRangeMutationEngine`
- `useDataGridA11yCellIds`
- `useDataGridColumnUiPolicy`
- `useDataGridEditableValuePolicy`
- `useDataGridMoveMutationPolicy`
- `useDataGridInlineEditorSchema`
- `useDataGridInlineEditOrchestration`
- `useDataGridInlineEditorTargetNavigation`
- `useDataGridInlineEditorKeyRouter`
- `useDataGridHeaderContextActions`
- `useDataGridCopyRangeHelpers`
- `useDataGridHeaderSortOrchestration`
- `useDataGridHeaderResizeOrchestration`
- `useDataGridHeaderInteractionRouter`
- `useDataGridColumnFilterOrchestration`
- `useDataGridEnumTrigger`
- `useDataGridGroupValueLabelResolver`
- `useDataGridGroupMetaOrchestration`
- `useDataGridGroupBadge`
- `useDataGridGroupingSortOrchestration`
- `useDataGridViewportMeasureScheduler`
- `useDataGridVisibleRowsSyncScheduler`
- `useDataGridColumnLayoutOrchestration`
- `useDataGridSelectionOverlayOrchestration`
- `useDataGridRowsProjection`
- `useDataGridRowSelectionOrchestration`
- `useDataGridRowSelectionInputHandlers`
- `useDataGridVirtualRangeMetrics`
- `useDataGridContextMenuAnchor`
- `useDataGridContextMenuActionRouter`
- `useDataGridViewportContextMenuRouter`
- `useDataGridViewportBlurHandler`
- `useDataGridViewportScrollLifecycle`
- `useDataGridLinkedPaneScrollSync`
- `useDataGridResizeClickGuard`
- `useDataGridInitialViewportRecovery`
- `useDataGridManagedWheelScroll`
- `useDataGridClearSelectionLifecycle`
- `useDataGridGlobalPointerLifecycle`
- `useDataGridPointerAutoScroll`
- `useDataGridPointerPreviewRouter`
- `useDataGridPointerCellCoordResolver`
- `useDataGridAxisAutoScrollDelta`
- `useDataGridCellVisibilityScroller`
- `useDataGridGlobalMouseDownContextMenuCloser`
- `useDataGridKeyboardCommandRouter`
- `useDataGridQuickFilterActions`
- `useDataGridCellCoordNormalizer`
- `useDataGridSelectionComparators`
- `useDataGridRowSelectionModel`
- `useDataGridPointerModifierPolicy`
- `useDataGridHistoryActionRunner`
- `useDataGridInlineEditorFocus`
- `useDataGridRowSelectionFacade`
- `useDataGridFindReplaceFacade`
- `useDataGridClipboardBridge`
- `useDataGridClipboardMutations`
- `useDataGridIntentHistory`

## Quick start

```ts
import { ref } from "vue"
import { useDataGridRuntime } from "@affino/datagrid-vue"

const rows = ref([])
const columns = ref([
  { key: "service", label: "Service", width: 220 },
])

const { api, columnSnapshot } = useDataGridRuntime({
  rows,
  columns,
})
```

## Managed wheel scroll (advanced)

Use `useDataGridManagedWheelScroll` when you want deterministic wheel ownership (axis lock, preventDefault policy, and consistent header/body horizontal sync).

```ts
import { useDataGridManagedWheelScroll } from "@affino/datagrid-vue/advanced"

const managedWheelScroll = useDataGridManagedWheelScroll({
  resolveWheelMode: () => "managed", // "managed" | "native"
  resolveWheelAxisLockMode: () => "dominant",
  resolvePreventDefaultWhenHandled: () => true,
  resolveBodyViewport: () => viewportRef.value,
  resolveMainViewport: () => viewportRef.value
    ? {
        scrollLeft: viewportRef.value.scrollLeft,
        scrollWidth: viewportRef.value.scrollWidth,
        clientWidth: viewportRef.value.clientWidth,
      }
    : null,
  setHandledScrollTop: (nextTop) => {
    if (viewportRef.value) {
      viewportRef.value.scrollTop = nextTop
    }
  },
  setHandledScrollLeft: (nextLeft) => {
    if (viewportRef.value) {
      viewportRef.value.scrollLeft = nextLeft
    }
  },
})

function onViewportWheel(event: WheelEvent) {
  managedWheelScroll.onBodyViewportWheel(event)
}
```

```vue
<div ref="viewportRef" @wheel="onViewportWheel" @scroll="onViewportScroll" />
```

- Call `managedWheelScroll.reset()` on unmount.
- Keep DOM reads/writes in adapter/demo/UI layer; do not move wheel DOM handling into core.

## Orchestration-heavy viewport integration

Use orchestration primitives from `@affino/datagrid-vue/advanced` to keep component code thin while preserving high-fidelity interaction behavior.

```ts
import {
  useDataGridLinkedPaneScrollSync,
  useDataGridResizeClickGuard,
  useDataGridInitialViewportRecovery,
  useDataGridRowSelectionModel,
} from "@affino/datagrid-vue/advanced"

const linkedPaneSync = useDataGridLinkedPaneScrollSync({
  resolveSourceScrollTop: () => viewportRef.value?.scrollTop ?? 0,
  mode: "css-var",
  resolveCssVarHost: () => gridRootRef.value,
})

const resizeGuard = useDataGridResizeClickGuard()

const viewportRecovery = useDataGridInitialViewportRecovery({
  resolveShouldRecover: () => totalRows.value > 1 && renderedRowsCount.value <= 1,
  runRecoveryStep: () => syncViewportMetrics(),
})

const rowSelectionModel = useDataGridRowSelectionModel({
  resolveFilteredRows: () => filteredRows.value,
  resolveRowId: row => String(row.rowId),
  resolveAllRows: () => allRows.value,
})
```

Recommended ownership:
- `@affino/datagrid-core`: deterministic data/runtime contracts.
- `@affino/datagrid-orchestration`: interaction policies and state orchestration.
- `@affino/datagrid-vue`: refs/template wiring and DOM lifecycle integration.

## 60-second integration (junior-friendly)

```ts
import { ref } from "vue"
import { AffinoDataGridSimple } from "@affino/datagrid-vue/components"

const rows = ref([
  { rowId: "1", service: "edge-gateway", owner: "NOC" },
  { rowId: "2", service: "billing-api", owner: "Payments" },
])

const columns = [
  { key: "service", label: "Service", width: 220 },
  { key: "owner", label: "Owner", width: 180 },
]
```

```vue
<AffinoDataGridSimple
  v-model:rows="rows"
  :columns="columns"
  :features="{ selection: true, clipboard: true, editing: true }"
/>
```

- Includes pre-wired sort, row-selection, context-menu, clipboard and inline edit.
- Emits `update:rows`, `update:status`, and `action` for app-level integration.

## High-level sugar API

```ts
import { ref } from "vue"
import { useAffinoDataGrid } from "@affino/datagrid-vue"

const rows = ref([
  { rowId: "1", service: "edge-gateway", owner: "NOC" },
  { rowId: "2", service: "billing-api", owner: "Payments" },
])

const columns = ref([
  { key: "service", label: "Service", width: 220 },
  { key: "owner", label: "Owner", width: 180 },
])

const grid = useAffinoDataGrid({
  rows,
  columns,
  features: {
    selection: true,
    clipboard: true,
    editing: {
      mode: "cell",
      enum: true,
    },
    filtering: {
      enabled: true,
      initialFilterModel: {
        columnFilters: {},
        advancedFilters: {},
      },
    },
    summary: {
      enabled: true,
      columns: [
        { key: "owner", aggregations: ["countDistinct"] },
      ],
    },
    visibility: {
      enabled: true,
      hiddenColumnKeys: [],
    },
    tree: {
      enabled: true,
      initialGroupBy: {
        fields: ["owner"],
        expandedByDefault: true,
      },
    },
    interactions: {
      enabled: true,
      range: { enabled: true, fill: true, move: true },
    },
    headerFilters: {
      enabled: true,
      maxUniqueValues: 300,
    },
    feedback: {
      enabled: true,
      maxEvents: 120,
    },
    statusBar: {
      enabled: true,
    },
    keyboardNavigation: true,
  },
})

type Grid = ReturnType<typeof useAffinoDataGrid>
// grid is fully typed and safe to destructure.
```

Row identity contract (required):

- Each row must expose a stable non-empty `rowId` (or `id`/`key`), or
- Provide `features.selection.resolveRowKey(row, index)`.
- Index-based fallback keys are intentionally not used.

```vue
<th v-for="column in columns" :key="column.key" v-bind="grid.bindings.headerCell(column.key)">
  {{ column.label }}
</th>

<td
  v-for="column in columns"
  :key="column.key"
  v-bind="grid.bindings.dataCell({ row, rowIndex, columnKey: column.key, value: row[column.key] })"
>
  <input
    v-if="grid.bindings.isCellEditing(String(row.rowId), column.key)"
    v-bind="grid.bindings.inlineEditor({ rowKey: String(row.rowId), columnKey: column.key })"
  />
  <span v-else>{{ row[column.key] }}</span>
</td>
```

- `grid.componentProps` can be passed into `<DataGrid v-bind="grid.componentProps" />`.
- `grid.bindings` provides ready wiring helpers:
  - `grid.bindings.headerSort(column.key)` for sortable header handlers/ARIA.
  - `grid.bindings.rowSelection(row, rowIndex)` for row selection click/keyboard behavior.
  - `grid.bindings.editableCell({ row, rowIndex, columnKey })` + `grid.bindings.inlineEditor(...)` for inline edit flows.
  - `grid.bindings.headerCell(column.key)` and `grid.bindings.dataCell(...)` for pre-wired sort/edit + context-menu behavior.
  - `grid.bindings.contextMenuRoot()` + `grid.bindings.contextMenuAction(action.id)` for menu keyboard/click wiring.
  - `grid.bindings.actionButton("copy" | "cut" | "paste" | ...)` for toolbar-level actions.
- `grid.actions` provides no-router commands for common flows:
  - `runAction("copy" | "cut" | "paste" | "clear" | "sort-asc" | "sort-desc" | "sort-clear")`
  - `copySelectedRows()`, `cutSelectedRows()`, `pasteRowsAppend()`, `clearSelectedRows()`, `selectAllRows()`
  - Mutating clipboard flows (`clear/cut/paste`) are intent-transaction backed in sugar path (undo/redo-capable when history controls are wired).
- `grid.contextMenu` wraps menu state + keyboard support + action execution:
  - `open(x, y, { zone, columnKey?, rowId? })`
  - `runAction(actionId)` (maps directly into `grid.actions`)
- `grid.features.filtering` exposes `model`, `setModel`, `setAdvancedExpression`, and `clear`.
- `grid.features.filtering.helpers` provides typed advanced-filter helpers:
  - `setText`, `setNumber`, `setDate`, `setSet`, `apply`, `clearByKey`
  - merge modes: `replace | merge-and | merge-or`
  - set value modes: `replace | append | remove`
- `grid.features.summary.selected` returns deterministic aggregates for current selection scope.
- `grid.features.visibility` exposes `setColumnVisible`, `toggleColumnVisible`, `setHiddenColumnKeys`, `reset`.
- `grid.features.tree` exposes `groupBy`, `groupExpansion`, `setGroupBy`, `toggleGroup`, `expandAll`, `collapseAll`.
- `grid.features.rowHeight` exposes `setMode("fixed" | "auto")`, `setBase(height)`, `measureVisible()`.
- `features.keyboardNavigation: true` enables out-of-the-box shortcuts and cell navigation:
  - `Cmd/Ctrl+C`, `Cmd/Ctrl+X`, `Cmd/Ctrl+V`, `Delete/Backspace`
  - `Cmd/Ctrl+Z`, `Cmd/Ctrl+Shift+Z`, `Cmd/Ctrl+Y`
  - arrows/home/end/page/tab/enter range navigation on focused grid cells
- `grid.pagination` exposes first/prev/next/last + snapshot wrappers.
- `grid.columnState` exposes `capture/apply` and point updates (`setOrder`, `setVisibility`, `setWidth`, `setPin`).
- `grid.history` exposes `supported/canUndo/canRedo/undo/redo`.
- `grid.rowReorder` exposes guarded client-side reorder (`moveByIndex`, `moveByKey`).
- `grid.cellSelection` exposes anchor/focus/range model (`setCellByKey`, `isCellSelected`, `clear`).
- `grid.cellRange` exposes range clipboard/fill/move (`copy`, `cut`, `paste`, `clear`, `applyFillPreview`, `applyRangeMove`).
- `grid.features.interactions.range` provides declarative fill/move lifecycle flags.
- `grid.bindings.rangeHandle` + `grid.bindings.rangeSurface` wire fill/move without page-local pointer logic.
- `grid.bindings.columnResizeHandle` + `grid.bindings.rowResizeHandle` provide drag/keyboard/double-click autosize.
- `grid.features.headerFilters` provides Excel-style popover model (`open/toggle/query/operators/unique-values/select-only/select-all`).
- `grid.feedback` exposes unified event stream for action/context/history/range/header-filter flows.
- `grid.contextMenu` parity helpers: `openForActiveCell`, `openForHeader`, `groupedActions`, disabled reason helpers.
- `grid.features.editing.enumEditor` exposes Affino enum-editor contract (`primitive`, `resolveOptions`).
- `grid.layoutProfiles` exposes save/apply/remove/clear for sort/filter/group/column snapshots.
- `grid.statusBar` exposes built-in metrics model and summary accessors for status bar UIs.

## Complete integration playbook

For end-to-end integration (tree rendering contract, advanced-filter AST cookbook, interaction/hotkey contract, and full-page setup), use:

- `/Users/anton/Projects/affinio/docs/datagrid-vue-sugar-playbook.md`
- `/Users/anton/Projects/affinio/docs/datagrid-sheets-user-interactions-and-integrator-api.md`

## Junior-first UI wrapper

```ts
import { ref } from "vue"
import { useAffinoDataGridUi } from "@affino/datagrid-vue"

const status = ref("Ready")
const grid = useAffinoDataGridUi({
  rows,
  columns,
  status,
  features: {
    selection: true,
    clipboard: true,
    editing: true,
  },
})
```

```vue
<button v-bind="grid.ui.bindToolbarAction('copy')">Copy</button>
<th v-bind="grid.ui.bindHeaderCell(column.key)">{{ column.label }}</th>
<td v-bind="grid.ui.bindDataCell({ row, rowIndex, columnKey: column.key, value: row[column.key] })">
  <input
    v-if="grid.ui.isCellEditing(String(row.rowId), column.key)"
    v-bind="grid.ui.bindInlineEditor({ rowKey: String(row.rowId), columnKey: column.key })"
  />
</td>
```
- Demo-level orchestration can be imported from `@affino/datagrid-vue/internal`.
