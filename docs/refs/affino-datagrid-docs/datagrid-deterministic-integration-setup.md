# DataGrid Deterministic Integration Setup

Baseline date: `2026-02-08`

This guide defines the stable integration path for pinned/overlay/viewport state.

## 1. Core Viewport Snapshot (Single Source of Truth)

```ts
import { createDataGridViewportController } from "@affino/datagrid-core/advanced"

const viewport = createDataGridViewportController({ resolvePinMode })
viewport.attach(containerEl, headerEl)
viewport.setViewportMetrics({
  containerWidth: 1280,
  containerHeight: 720,
  headerHeight: 52,
})
viewport.refresh(true)

const state = viewport.getIntegrationSnapshot()
```

Read integration data only from `state`:
- `state.visibleRowRange`
- `state.visibleColumnRange`
- `state.pinnedWidth`
- `state.overlaySync`

## 2. Vue Overlay Transform from Public Helper

```ts
import { buildDataGridOverlayTransformFromSnapshot } from "@affino/datagrid-vue"

const transform = buildDataGridOverlayTransformFromSnapshot({
  viewportWidth: state.viewportWidth,
  viewportHeight: state.viewportHeight,
  scrollLeft: state.scrollLeft,
  scrollTop: state.scrollTop,
  pinnedOffsetLeft: state.overlaySync.pinnedOffsetLeft,
  pinnedOffsetRight: state.overlaySync.pinnedOffsetRight,
})
```

Use `transform` as the only source for overlay layer positioning.

## 3. Integration Rules

- Do not read private component refs or DOM transforms for geometry logic.
- Do not compute pinned offsets in product code if snapshot already provides them.
- Keep stable imports at package root and power-user imports at advanced entrypoint:
  - `@affino/datagrid-core`
  - `@affino/datagrid-core/advanced`
  - `@affino/datagrid-vue`
- On repeated refresh without state delta, integration output must remain unchanged.
