# DataGrid Viewport Math Engine

Updated: `2026-02-07`

`D5` isolates viewport calculations into pure deterministic functions and keeps DOM writes in IO boundary services.

## Pure Math Module

File: `packages/datagrid-core/src/viewport/dataGridViewportMath.ts`

Implemented deterministic helpers:

- `resolveViewportDimensions(...)`
- `resolvePendingScroll(...)`
- `shouldUseFastPath(...)`
- `resolveHorizontalSizing(...)`
- `computePinnedWidth(...)`
- `shouldNotifyNearBottom(...)`

These functions:

- have no DOM/framework dependencies
- contain no runtime side effects
- keep controller orchestration focused on wiring and lifecycle

## IO Boundary

File: `packages/datagrid-core/src/viewport/dataGridViewportScrollIo.ts`

Added `applyProgrammaticScrollWrites(...)` so programmatic scroll writes are performed in the scroll IO service, not in controller orchestration.

This keeps:

- viewport math in pure modules
- scroll read/write effects inside dedicated host boundary

## Contract Tests

Added:

- `packages/datagrid-core/src/viewport/__tests__/dataGridViewportMath.contract.spec.ts`

Updated:

- `packages/datagrid-core/src/viewport/__tests__/scrollSync.raf.spec.ts`

Contracts cover determinism, fallback behavior, and IO boundary programmatic scroll writes.
