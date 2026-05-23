# DataGrid Strict Contract Testing Matrix

Updated: `2026-02-08`

This matrix defines mandatory contract suites for `@affino/datagrid-core` and `@affino/datagrid-vue`.

## Entry Commands

- root: `pnpm run test:datagrid:strict-contracts`
- core: `pnpm --filter @affino/datagrid-core test:strict-contracts`
- vue: `pnpm --filter @affino/datagrid-vue test:strict-contracts`

Pattern gate:
- `contract|lifecycle|property|stress|determinism`

## Required Coverage Domains

Models:
- contract/lifecycle: model contracts and API boundaries
- property: randomized invariant checks
- stress: sustained high-volume command sequences
- determinism: equal command sequence -> equal snapshots

Boundaries:
- viewport/model bridge contracts
- property-based boundary mapping checks
- stress regressions on virtualization windows
- deterministic replay under repeated refresh/resize/scroll

## Canonical Suites

Core model/boundary suites:
- `packages/datagrid-core/src/models/__tests__/determinism.contract.spec.ts`
- `packages/datagrid-core/src/models/__tests__/clientRowModel.property.spec.ts`
- `packages/datagrid-core/src/models/__tests__/clientRowModel.stress.spec.ts`
- `packages/datagrid-core/src/viewport/__tests__/modelBridge.property.contract.spec.ts`
- `packages/datagrid-core/src/viewport/__tests__/horizontalVirtualization.stress.contract.spec.ts`
- `packages/datagrid-core/src/viewport/__tests__/scrollResizeDeterminism.contract.spec.ts`
- `packages/datagrid-core/src/protocol/__tests__/versionedPublicProtocol.contract.spec.ts`
- `packages/datagrid-core/src/protocol/__tests__/publicProtocolCodemod.contract.spec.ts`
- `packages/datagrid-core/src/protocol/__tests__/entrypointTiers.contract.spec.ts`

Adapter suites:
- `packages/datagrid-vue/src/composables/__tests__/selectionOverlayTransform.contract.spec.ts`
- `packages/datagrid-vue/src/adapters/__tests__/selectionControllerAdapter.contract.spec.ts`
- `packages/datagrid-core/src/a11y/__tests__/headlessA11yStateMachine.contract.spec.ts`
- `packages/datagrid-vue/src/adapters/__tests__/a11yAttributesAdapter.contract.spec.ts`

## Quality Lock

- `quality:lock:datagrid` includes strict contracts as fail-fast prerequisite.
- CI `quality-gates` job is blocking when strict contract suite fails.
