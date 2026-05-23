# DataGrid Vue Stable Entrypoint (Common Usage)

Updated: `2026-05-20`

This document defines the stable, semver-safe entrypoints for common `@affino/datagrid-vue` integrations.

## Stable Entrypoints

- Primary: `@affino/datagrid-vue`
- Explicit alias: `@affino/datagrid-vue/stable`
- Declarative app layer: `@affino/datagrid-vue-app`

`@affino/datagrid-vue` and `@affino/datagrid-vue/stable` are contract-equivalent.

## Stable Surface

- Core model and helper facade:
  - common row model factories, column model helpers, cell type helpers, cell render-model helpers, selection summary helpers, filter/sort helpers, datasource types, formula types, pivot types, and `DataGridApi` types re-exported from `@affino/datagrid-core`
  - pivot spec helpers re-exported from `@affino/datagrid-pivot`: `normalizePivotSpec`, `clonePivotSpec`, `isSamePivotSpec`
- Runtime/base:
  - `createDataGridVueRuntime`
  - `useDataGridRuntime`
  - `createGrid`
  - `useAffinoGrid`
  - Pivot utilities through `useDataGridRuntime`: `setPivotModel`, `getPivotModel`, `getPivotCellDrilldown`, `exportPivotLayout`, `importPivotLayout`, `exportPivotInterop`
- Settings, overlays, selectors, context, and a11y:
  - `useDataGridSettingsStore`
  - `createDataGridSettingsAdapter`
  - `buildDataGridOverlayTransform`
  - `buildDataGridOverlayTransformFromSnapshot`
  - `useDataGridSelectionOverlayOrchestration`
  - `mapDataGridA11yGridAttributes`
  - `mapDataGridA11yCellAttributes`
  - `useDataGridContextMenu`
  - `DATA_GRID_CLASS_NAMES`
  - `DATA_GRID_DATA_ATTRS`
  - `DATA_GRID_SELECTORS`
  - `dataGridCellSelector`
  - `dataGridHeaderCellSelector`
  - `dataGridResizeHandleSelector`
  - `provideDataGridEngineContext`, `useDataGridEngineContext`, `useGridApi`
  - `provideDataGridViewContext`, `useDataGridViewContext`
  - `provideDataGridContext`, `useDataGridContext`

The stable root intentionally includes the integration primitives above because existing app and wrapper integrations use them without opting into low-level pointer/editing/viewport internals. Low-level orchestration hooks remain outside root/stable.

Advanced hooks are available only via:
- `@affino/datagrid-vue/advanced`

## 60-Second Setup (Recommended)

```ts
import { ref } from "vue"
import { DataGrid } from "@affino/datagrid-vue-app"

const rows = ref([
  { rowId: "1", service: "edge-gateway", owner: "NOC" },
  { rowId: "2", service: "billing-api", owner: "Payments" },
])

const columns = [
  { key: "service", label: "Service", initialState: { width: 220 } },
  { key: "owner", label: "Owner", initialState: { width: 180 } },
]

const columnState = ref(null)
const gridState = ref(null)
```

```vue
<DataGrid
  :rows="rows"
  :columns="columns"
  v-model:column-state="columnState"
  v-model:state="gridState"
/>
```

This app-layer path is also the supported spreadsheet fill integration: the default renderer exposes fill-handle drag-fill, double-click fill-down, and the post-fill `Series` / `Copy` menu in base table mode.

The same app-layer path is also the supported additive toolbar extension path: `DataGrid` exposes a public `toolbarModules` prop for app-specific toolbar buttons and popovers without replacing the built-in renderer.

```ts
import {
  DataGrid,
  type DataGridAppToolbarModule,
} from "@affino/datagrid-vue-app"
```

Prefer `toolbarModules` when you want to append actions to the built-in toolbar.
Use the default slot only when you need full runtime-renderer ownership.

## If You Need More Control

```ts
import { useDataGridRuntime } from "@affino/datagrid-vue"
```

Use `useDataGridRuntime` for headless runtime ownership. Build interaction/UI behavior with `@affino/datagrid-vue-app` or `@affino/datagrid-vue/advanced` hooks.

## Contract Guard

- Stable/runtime contract coverage lives in:
  - `packages/datagrid-vue/src/composables/__tests__/useDataGridRuntime.contract.spec.ts`
  - `packages/datagrid-vue/src/__tests__/entrypointTiers.contract.spec.ts`
- Run package contract suite:
  - `pnpm --filter @affino/datagrid-vue run test:contracts`

## Removed Legacy Aliases

Legacy aliases were removed from package code and are no longer supported:

- `useTableSettingsStore`
- `createPiniaTableSettingsAdapter`
- `buildSelectionOverlayTransform`
- `buildSelectionOverlayTransformFromSnapshot`

Use only canonical names from the stable surface.
