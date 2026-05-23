# DataGrid Migration Guide

Baseline date: `2026-02-08`
Migration path: `.tmp/ui-table` -> `@affino/datagrid-core` + `@affino/datagrid-vue`

## Migration Scope

- Runtime and architecture source of truth moves from temporary tree:
  `.tmp/ui-table`
- Product integration target moves to package surfaces:
  - `packages/datagrid-core`
  - `packages/datagrid-vue`

## Import Mapping

| Legacy | New target |
| --- | --- |
| `.tmp/ui-table/core/*` | `@affino/datagrid-core` (root public API for stable usage) |
| `.tmp/ui-table/core/viewport/*` and runtime internals | `@affino/datagrid-core/advanced` |
| `.tmp/ui-table/vue/*` | `@affino/datagrid-vue` |
| `UiTable*` naming | `DataGrid*` naming (legacy aliases removed) |

## Naming Migration (Vue)

- `UiTable.vue` -> `DataGrid.vue`
- `UiTableViewportSimple.vue` -> `DataGridViewport.vue`
- `UiTableOverlayLayer.vue` -> `DataGridOverlayLayer.vue`

Compatibility shims are removed from current package surface.
Use `DataGrid*` names only.

## Pinning Migration (Required)

Runtime canonical pin contract:
- `pin: "left" | "right" | "none"`

Migration rule:
- Use canonical `pin` only.
- Use canonical `pin` fields in runtime and adapter input.

## Column Authored Input Migration (Required)

New contract split:

- `DataGridColumnDef` = immutable definition
- `DataGridColumnInput` = definition + `initialState`
- `DataGridColumnState` = runtime mutable state

Required move for authored columns:

```ts
// before
{ key: "amount", label: "Amount", width: 140, pin: "right", visible: true }

// after
{
   key: "amount",
   label: "Amount",
   initialState: { width: 140, pin: "right", visible: true },
}
```

## Recommended Migration Steps

1. Move imports from `.tmp/ui-table/*` to package-root imports.
2. Switch component usage to `DataGrid*` names.
3. Normalize pinning input to canonical `pin` before runtime.
4. Replace ad-hoc overlay/scroll math with deterministic public contracts:
   - `createDataGridViewportController(...).getIntegrationSnapshot()` from `@affino/datagrid-core/advanced`
   - `buildDataGridOverlayTransform(...)` or `buildDataGridOverlayTransformFromSnapshot(...)`
5. Remove direct dependency on legacy internal path aliases.
6. Re-run quality/perf gates before release.

## Core Deep Import Migration

`@affino/datagrid-core` only exports the tiered entrypoints `.`, `./advanced`, and `./internal`. Source-shaped deep imports such as `@affino/datagrid-core/src/*` or `@affino/datagrid-core/viewport/*` are unsupported and blocked by the package export map.

Migration rule:

- Stable model, API, event, datasource, formula, spreadsheet, and selection helpers should import from `@affino/datagrid-core`.
- Viewport controller, transaction service, adapter runtime, a11y state machine, and datasource-backed row model power-user APIs should import from `@affino/datagrid-core/advanced`.
- Unsafe normalization helpers should import from `@affino/datagrid-core/internal` only in package-internal or explicitly advanced integration code.

## GridApi Namespace Migration (Flat -> Namespaced)

Preferred API shape:

- `api.lifecycle`
- `api.rows.*`
- `api.data.*`
- `api.columns.*`
- `api.view.*`
- `api.pivot.*`
- `api.selection.*`
- `api.transaction.*`
- `api.compute.*`
- `api.diagnostics.*`
- `api.meta.*`
- `api.policy.*`
- `api.plugins.*`
- `api.state.*`
- `api.events.*`
- `api.capabilities`

Legacy flat methods are removed from `DataGridApi`.

Type alias cleanup:

- `CreateDataGridApiDependencies` was removed. Use `CreateDataGridApiFromDepsOptions`.
- `resolvePreventDefaultWhenConsumed` was removed from managed wheel scroll options. Use `resolvePreventDefaultWhenHandled`.
- Worker compute request messages no longer accept direct payloads. Use the schema-versioned payload envelope.

Examples:

- `api.setColumnWidth(key, width)` -> `api.columns.setWidth(key, width)`
- `api.getRowCount()` -> `api.rows.getCount()`
- `api.getRowsInRange(range)` -> `api.rows.getRange(range)`
- `api.setPivotModel(model)` -> `api.pivot.setModel(model)`
- `api.getSelectionSnapshot()` -> `api.selection.getSnapshot()`
- `api.applyTransaction(tx)` -> `api.transaction.apply(tx)`
- `api.refresh()` -> `api.view.refresh()`

Semantics:

- `api.rows.applyEdits(...)` mutates data.
- `api.view.reapply()` only recomputes projection.

Enforcement:

- flat usage baseline lock: `pnpm run quality:api:datagrid:flat`
- baseline file: `docs/quality/datagrid-flat-api-baseline.json`
- public export inventory lock: `pnpm run quality:api:datagrid:inventory`
- declaration-level API report lock: `pnpm run quality:api:datagrid:report`

When public types change, review semver impact before refreshing `docs/quality/datagrid-api-report.json`.

## GroupBy-only -> TreeData Migration

Use this path when hierarchy is intrinsic (parent/path), not analytical grouping.

1. Keep `groupBy` only for value-based analytical grouping.
2. Move hierarchical projection into row model via `initialTreeData`:
   - `mode: "path"` with `getDataPath(...)`, or
   - `mode: "parent"` with `getParentId(...)`.
3. Ensure stable `rowId` for every leaf row (inject resolver if needed).
4. Make policies explicit:
   - `orphanPolicy`,
   - `cyclePolicy`,
   - `filterMode`.
5. Replace adapter-level hierarchy workarounds with row-model expansion API:
   - `toggleGroup`, `expandGroup`, `collapseGroup`, `expandAllGroups`, `collapseAllGroups`.
6. Validate tree quality gates:
   - `pnpm run test:datagrid:tree:contracts`
   - `pnpm run bench:datagrid:tree:assert`

Reference:

- `docs/datagrid-tree-data.md`
- `docs/datagrid-tree-data-behavior-matrix.md`

## Aggregation Migration Notes

Runtime API:

- `rowModel.setAggregationModel(model | null)`
- `rowModel.getAggregationModel()`
- `api.rows.setAggregationModel(model | null)`
- `api.rows.getAggregationModel()`

Recommended model:

- Use `basis: "filtered"` for "totals for current view" UX.
- Use `basis: "source"` when totals must remain global under active filters.

Important caveat:

- `patchRows(...)` is Excel-like by default: `recomputeSort/filter/group` are off unless explicitly enabled.
- In no-recompute modes, projection can be intentionally stale.
- Example: if aggregate-related patch is applied while aggregate recompute is blocked, `groupMeta.aggregates` remains stale until next allowed recompute/`refresh()`.

API-level edit flow:

- `api.rows.patch(updates, options)` forwards explicit patch policy flags.
- `api.rows.applyEdits(updates, { reapply? })` uses Excel-like defaults and optional live reapply.
- `api.rows.setAutoReapply(boolean)` / `api.rows.getAutoReapply()` control default apply-edits behavior.
- `api.view.reapply()` explicitly reapplies sort/filter/group projection.

## Verification Checklist

- Public imports only (no direct `src/*` internals for production integration).
- No runtime dependency on noncanonical pin fields.
- Integration reads pinned/overlay state via snapshot contract (`getIntegrationSnapshot`) instead of private refs.
- Overlay and selection geometry remain aligned during horizontal scroll.
- X-virtualization deterministic under resize/teleport-like scroll jumps.

## Validation Commands

- `pnpm run test:matrix:unit`
- `pnpm run test:matrix:integration`
- `pnpm run quality:gates:datagrid`
- `pnpm run bench:datagrid:harness:ci`

## Codemod Assist

For protocol-safe import migration and deprecated viewport API rename:

- dry run: `pnpm run codemod:datagrid:public-protocol -- <path>`
- write mode: `pnpm run codemod:datagrid:public-protocol -- --write <path>`

Codemod script:
- `scripts/codemods/datagrid-public-protocol-codemod.mjs`

## Rollback Strategy

- Roll back only by reverting migration commit(s).
- No compatibility shim window is maintained in package code.
