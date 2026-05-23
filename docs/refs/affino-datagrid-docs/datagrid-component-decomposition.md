# DataGrid Component Decomposition

Baseline date: `2026-02-07`

This note documents the `DataGrid.vue` decomposition pass that reduces orchestration pressure in one SFC.

## Feature Facades

`DataGrid.vue` now delegates cross-feature orchestration to dedicated facades:

- `src/features/useDataGridHeaderOrchestration.ts`
  - header layout orchestration wrapper (`useTableHeaderLayout`)
  - header bindings assembly facade (`createDataGridHeaderBindings`)
- `src/features/useDataGridRowSelectionFacade.ts`
  - row-selection model bridge (`useSelectableRows`)
  - checkbox indeterminate sync
  - row selection helper actions/class resolution
- `src/features/useDataGridFindReplaceFacade.ts`
  - find/replace bridge wrapper
- `src/features/useDataGridViewportBridge.ts`
  - viewport scroll event coalescing
  - scroll sync payload propagation to overlay snapshot

## Watcher Side-Effect Cleanup

Cross-feature watchers moved out of `DataGrid.vue` into facades:

- selection enable/disable reset watcher
- header checkbox indeterminate watcher
- viewport scroll-frame scheduling state (`requestAnimationFrame` queue + cancellation)

Result: side-effects are scoped to feature ownership modules instead of shared SFC scope.

## Integration Boundary

`DataGrid.vue` keeps composition orchestration, but feature behaviors are exposed through thin facade hooks.  
This keeps runtime behavior stable while reducing coupling between header/selection/find-replace/viewport concerns.
