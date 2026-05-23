# DataGrid Troubleshooting Runbook

Baseline date: `2026-02-07`
Scope: overlay, pinned columns, virtualization, selection sync

## Incident Triage Order

1. Reproduce with deterministic dataset and stable viewport size.
2. Identify failing invariant (transform owner, pin canonical state, coordinate space, clamp determinism).
3. Run focused contract tests.
4. Validate quality gates and performance harness.

## Symptom: Overlay drifts from cells while scrolling

Checks:
- Verify there is no duplicate transform owner in adapter layer.
- Verify sync targets are explicit refs, not DOM fallback queries.

Relevant tests:
- `packages/datagrid-core/src/viewport/__tests__/scrollSync.transforms.spec.ts`
- `packages/datagrid-core/src/viewport/__tests__/syncTargets.contract.spec.ts`

## Symptom: Pinned columns overlap or jump during scroll/resize

Checks:
- Confirm runtime receives canonical `pin` only.
- Confirm host column pin fields are normalized to canonical `pin` before reaching core.

Relevant tests:
- `packages/datagrid-core/src/columns/__tests__/pinning.spec.ts`
- `packages/datagrid-vue/src/adapters/__tests__/columnPinNormalization.spec.ts`

## Symptom: Horizontal virtualization not deterministic

Checks:
- Confirm clamp logic is served only by `dataGridViewportHorizontalClamp.ts`.
- Confirm prepare/update path does not mutate input metadata.

Relevant tests:
- `packages/datagrid-core/src/viewport/__tests__/horizontalClamp.contract.spec.ts`
- `packages/datagrid-core/src/viewport/__tests__/horizontalUpdate.contract.spec.ts`
- `packages/datagrid-core/src/viewport/__tests__/horizontalVirtualization.stress.contract.spec.ts`

## Symptom: Fill handle and selection rectangles are desynced

Checks:
- Confirm world/viewport/client coordinate conversions use canonical helpers.
- Confirm overlay updates are split by dirty signatures, not full rebuilds.

Relevant tests:
- `packages/datagrid-core/src/selection/__tests__/coordinateSpace.contract.spec.ts`
- `packages/datagrid-core/src/selection/__tests__/overlay.fill.geometry.contract.spec.ts`

## Symptom: Selection controller behaves differently after remount

Checks:
- Verify lifecycle calls order: `init` -> repeated `sync` -> `teardown`.
- Verify no stale refs remain after unmount.

Relevant tests:
- `packages/datagrid-vue/src/adapters/__tests__/selectionControllerAdapter.contract.spec.ts`
- `packages/datagrid-vue/src/composables/selection/__tests__/selectionDecomposition.integration.spec.ts`

## Standard Validation Commands

- `pnpm run test:matrix:unit`
- `pnpm run test:matrix:integration`
- `pnpm run quality:gates:datagrid`
- `pnpm run bench:datagrid:harness:ci`

## Escalation Criteria

Escalate as release blocker if any apply:
- Overlay/cell mismatch reproducible in deterministic scenario.
- Pinned columns produce wrong hit target or visual overlap.
- Horizontal virtualization fails stress contract.
- Performance harness breaks fail-fast budgets.

## Recovery Actions

- Roll back to last green commit where quality and performance gates passed.
- Keep compatibility shims enabled during rollback window.
- Re-open incident with failing invariant and exact spec path.
