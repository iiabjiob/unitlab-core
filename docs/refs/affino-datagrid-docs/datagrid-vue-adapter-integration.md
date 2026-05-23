# DataGrid Vue Adapter Integration Guide

Baseline date: `2026-02-09`
Package: `@affino/datagrid-vue`

## Current Stable Contract

As of this baseline, the stable public adapter API covers runtime, settings persistence, deterministic overlay transforms, and app-shell foundations:

- `createDataGridVueRuntime`
- `useDataGridRuntime`
- `createDataGridSettingsAdapter`
- `useDataGridSettingsStore`
- `buildDataGridOverlayTransform`
- `buildDataGridOverlayTransformFromSnapshot`
- `mapDataGridA11yGridAttributes`
- `mapDataGridA11yCellAttributes`
- `useDataGridContextMenu`

Pivot interop is included on stable runtime API:

- `setPivotModel` / `getPivotModel`
- `getPivotCellDrilldown`
- `exportPivotLayout` / `importPivotLayout`
- `exportPivotInterop`

Cross-platform runtime protocol (from core):

- `createDataGridAdapterRuntime`
- `resolveDataGridAdapterEventName`
- `DataGridAdapterRuntime`

Import path for protocol/runtime/viewport power-user APIs:
- `@affino/datagrid-core/advanced`

Source: `packages/datagrid-vue/src/public.ts`

Declarative component layer:
- `@affino/datagrid-vue-app`

## Quick Start (Recommended: Declarative Component)

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

```html
<DataGrid
  :rows="rows"
  :columns="columns"
  v-model:column-state="columnState"
  v-model:state="gridState"
/>
```

## Runtime/Overlay Quick Start (Power-User)

```ts
import {
  createDataGridVueRuntime,
  useDataGridRuntime,
  createDataGridSettingsAdapter,
  useDataGridSettingsStore,
  buildDataGridOverlayTransformFromSnapshot,
} from "@affino/datagrid-vue"
import { createDataGridViewportController } from "@affino/datagrid-core/advanced"

const runtime = createDataGridVueRuntime({ rows, columns })
await runtime.api.start()

const { api, columnSnapshot } = useDataGridRuntime({ rows, columns })

const store = useDataGridSettingsStore()
const settingsAdapter = createDataGridSettingsAdapter(store)

// pass settingsAdapter into your grid config where DataGridSettingsAdapter is expected

const viewport = createDataGridViewportController({ resolvePinMode })
const integration = viewport.getIntegrationSnapshot()

const overlayTransform = buildDataGridOverlayTransformFromSnapshot({
  viewportWidth: integration.viewportWidth,
  viewportHeight: integration.viewportHeight,
  scrollLeft: integration.scrollLeft,
  scrollTop: integration.scrollTop,
  pinnedOffsetLeft: integration.overlaySync.pinnedOffsetLeft,
  pinnedOffsetRight: integration.overlaySync.pinnedOffsetRight,
})
```

Adapter interface is defined in:
`packages/datagrid-core/src/dataGridSettingsAdapter.ts`

## What the Adapter Persists

- Column widths
- Sorting state
- Filter snapshot
- Pin state
- Group state

Store implementation:
`packages/datagrid-vue/src/dataGridSettingsStore.ts`

## Integration Rules

- Use tiered imports:
  - stable: package-root imports
  - power-user runtime/viewport: `@affino/datagrid-core/advanced`
- Treat `src/*` imports as internal unless explicitly versioned later.
- Keep pin input canonical (`pin`) by the time data reaches runtime.
- Read runtime geometry from `viewport.getIntegrationSnapshot()` instead of direct DOM probing.
- Build overlay transforms through `buildDataGridOverlayTransform*` helpers.
- Keep adapter lifecycle explicit for controller bridges: `init`, `sync`, `teardown`, `diagnostics`.
- Expose plugin integrations through `pluginContext.getCapabilityMap()` (no direct host-expose passthrough).

Lifecycle modules:
- `packages/datagrid-vue/src/adapters/adapterLifecycle.ts`
- `packages/datagrid-vue/src/adapters/selectionHeadlessAdapter.ts`
- `packages/datagrid-vue/src/adapters/selectionControllerAdapter.ts`

## SSR and Storage Notes

- Store uses `localStorage` in browser.
- During SSR/non-browser contexts, it falls back to in-memory storage shim.

## Integration Test Coverage to Keep Green

- Adapter contract: mount/unmount/remount/hydration
  - `packages/datagrid-vue/src/adapters/__tests__/selectionControllerAdapter.contract.spec.ts`
- Pin normalization contract
  - `packages/datagrid-vue/src/adapters/__tests__/columnPinNormalization.spec.ts`

## Troubleshooting Link

For overlay/pinned/virtualization incidents see:
`docs/datagrid-troubleshooting-runbook.md`

Deterministic setup reference:
`docs/datagrid-deterministic-integration-setup.md`

Cross-platform adapter protocol reference:
`docs/datagrid-cross-platform-adapter-protocol.md`

Sugar-first integration playbook (recommended for product teams):
`docs/datagrid-vue-sugar-playbook.md`
