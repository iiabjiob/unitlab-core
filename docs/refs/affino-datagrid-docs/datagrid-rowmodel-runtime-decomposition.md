# DataGrid RowModel Runtime Decomposition

Updated: `2026-03-01`  
Scope: `@affino/datagrid-core` client row model internals.

## Why

`createClientRowModel` accumulated multiple subsystems:

- projection orchestration,
- tree projection caches + subtree toggles,
- pivot projection runtime,
- incremental aggregation plumbing.

This document fixes ownership boundaries so perf work does not increase architectural entropy.

## Runtime Split Plan

- [x] `Phase A`: extract Pivot subsystem into dedicated runtime module.
  - module: `packages/datagrid-core/src/models/pivotRuntime.ts`
  - `clientRowModel` now orchestrates pivot stage by calling runtime API:
    - `pivotRuntime.projectRows(...)`
    - `pivotRuntime.normalizeColumns(...)`
- [x] `Phase B`: extract Tree projection subsystem (`path` / `parent` caches, subtree toggle fast-path).
  - module: `packages/datagrid-core/src/models/treeProjectionRuntime.ts`
  - `clientRowModel` now delegates tree cache/projection operations:
    - `treeProjectionRuntime.buildCacheKey(...)`
    - `treeProjectionRuntime.projectRowsFromCache(...)`
    - `treeProjectionRuntime.tryProjectPathSubtreeToggle(...)`
    - `treeProjectionRuntime.tryProjectParentSubtreeToggle(...)`
    - `treeProjectionRuntime.patchPathCacheRowsByIdentity(...)`
    - `treeProjectionRuntime.patchParentCacheRowsByIdentity(...)`
- [x] `Phase C`: extract Incremental aggregation adapter (`group` / `tree`, later `pivot`).
  - module: `packages/datagrid-core/src/models/incrementalAggregationRuntime.ts`
  - extracted:
    - group-by incremental state build (`computeGroupByIncrementalAggregation(...)`)
    - group-by incremental delta apply
    - tree path incremental delta apply
    - tree parent incremental delta apply
    - unified patch gate (`applyIncrementalAggregationPatch(...)`)
- [x] `Phase D`: keep `createClientRowModel` as orchestration shell only (state + stage wiring + lifecycle).
  - [x] extracted row runtime utility layer:
    - module: `packages/datagrid-core/src/models/clientRowRuntimeUtils.ts`
    - moved patch/remap/order/reindex/versioning helpers out of `clientRowModel`.
  - [x] extracted projection primitives layer:
    - module: `packages/datagrid-core/src/models/clientRowProjectionPrimitives.ts`
    - moved `filter/sort/signature/histogram` algorithms out of `clientRowModel`.
  - [x] final cleanup pass:
    - module: `packages/datagrid-core/src/models/clientRowModelHelpers.ts`
    - moved aggregate-map/snapshot helper functions out of `clientRowModel`.
    - `clientRowModel` retains only minimal local identity guard + orchestration wiring.

## Current Ownership

- `clientRowModel` keeps:
  - projection stage orchestration,
  - lifecycle/event emission,
  - cross-runtime invalidation policy decisions.
- `pivotRuntime` keeps:
  - pivot field resolution,
  - pivot key materialization,
  - pivot aggregation path (incremental fast-path + fallback),
  - runtime pivot rows/columns generation.
- `treeProjectionRuntime` keeps:
  - tree `path`/`parent` cache building,
  - cache materialization into projected rows,
  - subtree toggle fast-path projection,
  - cache row-identity patching.
- `incrementalAggregationRuntime` keeps:
  - group/tree incremental state build and delta application,
  - patch gating for incremental aggregation fast-path.
- `clientRowProjectionPrimitives` keeps:
  - filter predicate creation,
  - sort/signature/histogram primitives.
- `clientRowRuntimeUtils` keeps:
  - row patch/remap/order/reindex/version/cache-cap utilities.

## Non-Goals (for this split)

- No public API behavior changes.
- No UI-layer changes.
- No semantic changes to `patchRows` recompute policy.
