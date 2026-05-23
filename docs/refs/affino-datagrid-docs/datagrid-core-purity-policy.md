# DataGrid Pure Core Policy

Updated: `2026-02-08`

This policy defines the deterministic core boundary for `@affino/datagrid-core`.

## Scope

The deterministic core layer includes:

- `packages/datagrid-core/src/models/**` (excluding tests)
- `packages/datagrid-core/src/a11y/**` (excluding tests)
- `packages/datagrid-core/src/protocol/**` (excluding tests)
- `packages/datagrid-core/src/core/gridCore.ts`
- `packages/datagrid-core/src/core/gridApi.ts`
- `packages/datagrid-core/src/selection/snapshot.ts`

## Rules

- No framework bindings in core:
  - no `vue` imports
  - no Livewire-related imports
- No DOM coupling in core:
  - no `document`/`window`
  - no DOM element types (`HTMLElement`, `HTML*Element`, `DOMRect`)
- No non-deterministic runtime primitives in core:
  - no `setTimeout`, `setInterval`, `requestAnimationFrame`
  - no `Math.random`, `Date.now`, `performance.now`
- Core state transitions must be deterministic:
  - same initial state + same command sequence => same snapshots

## Boundary Ownership

- Core owns serializable state contracts and command semantics.
- View/adapters own DOM, framework reactivity, and rendering details.
- Adapter-specific payload may pass through `meta` channels but does not become core contract fields.

## Contract Coverage

- Purity boundary checks:
  - `packages/datagrid-core/src/core/__tests__/pureCoreBoundary.contract.spec.ts`
- Deterministic transition checks:
  - `packages/datagrid-core/src/models/__tests__/determinism.contract.spec.ts`
