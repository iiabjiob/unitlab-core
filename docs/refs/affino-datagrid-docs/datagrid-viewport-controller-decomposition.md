# DataGrid Viewport Controller Decomposition

Updated: `2026-02-07`

Viewport runtime is now split into explicit service boundaries; controller keeps orchestration only.

## Service Boundaries

- `scroll-io`: `packages/datagrid-core/src/viewport/dataGridViewportScrollIo.ts`
  - native scroll sampling
  - rAF sync scheduling
  - resize observer wiring
- `virtual-range`: `packages/datagrid-core/src/viewport/dataGridViewportVirtualization.ts`
  - vertical virtualization plan/apply
  - overscan and row pool policy
- `model-bridge`: `packages/datagrid-core/src/viewport/dataGridViewportModelBridgeService.ts`
  - row/column model subscriptions
  - cached materialization for render planning
- `render-sync`: `packages/datagrid-core/src/viewport/dataGridViewportRenderSyncService.ts`
  - sync target management
  - pinned offsets synchronization
  - overlay/host transform application

## Ownership Boundary

- `what-to-render`:
  - model bridge snapshots + virtualization ranges
  - horizontal/vertical prepared plans
- `how-to-render`:
  - scroll sync transforms
  - pending DOM scroll writes
  - pinned/overlay alignment

`dataGridViewportController` now composes these services and coordinates update phases.

## Regression Coverage

Added boundary and stress tests:

- `packages/datagrid-core/src/viewport/__tests__/modelBridge.contract.spec.ts`
- `packages/datagrid-core/src/viewport/__tests__/renderSync.contract.spec.ts`

Existing stress contracts remain active for full viewport flow:

- `packages/datagrid-core/src/viewport/__tests__/horizontalVirtualization.stress.contract.spec.ts`
