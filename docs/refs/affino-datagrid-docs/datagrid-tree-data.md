# DataGrid TreeData Guide

Updated: `2026-02-11`

This document describes the canonical `treeData` contract for `@affino/datagrid-core`, including policies, examples, failure modes, and migration from group-by-only setups.

## What TreeData Is

TreeData is a **row projection mechanism**:

- input: flat rows with hierarchy hints,
- output: deterministic flattened stream of `group` + `leaf` rows,
- expansion state controls visibility of descendants.

TreeData is not a UI feature. Rendering details (chevrons, indent, icons) stay in adapters.

## When to Use TreeData vs GroupBy

Use `treeData` when hierarchy is intrinsic to data:

- parent-child ownership trees,
- filesystem-like paths,
- org/project/service/shard hierarchies.

Use `groupBy` when hierarchy is analytical/derived from fields at runtime.

Both are row-model projection concerns, but they solve different product semantics.

## Canonical Contract

Hierarchy source must be exactly one:

1. Path mode:
   - `mode: "path"`
   - `getDataPath(row, index) => readonly (string | number)[]`
2. Parent mode:
   - `mode: "parent"`
   - `getParentId(row, index) => rowId | null | undefined`
   - optional `rootParentId`

Stable identity is mandatory:

- every leaf row must have stable `rowId`,
- index-based identity is forbidden in tree mode.

TreeData policies:

- `expandedByDefault` (`false` default),
- `orphanPolicy`: `root | drop | error` (default `root`),
- `cyclePolicy`: `ignore-edge | error` (default `ignore-edge`),
- `filterMode`: `leaf-only | include-parents | include-descendants` (default `include-parents`).

## Projection Pipeline

Canonical order:

`source -> tree index -> filter markers -> sibling sort -> flatten(expansion, filterPolicy) -> visible`

Key rules:

- tree index is built before flattening,
- sorting stays within sibling sets,
- deterministic tie-breaker is stable `rowId`.

## Filter Policies in Tree Mode

`leaf-only`:

- shows matching leaf rows only,
- no synthetic group/ancestor rows,
- rows are flattened for projection.

`include-parents`:

- shows matching leaves plus required ancestor chain.

`include-descendants`:

- matched node keeps full descendant subtree visible.

## Expansion API

Row model / Grid API operations:

- `setGroupExpansion(snapshot | null)`
- `toggleGroup(groupKey)`
- `expandGroup(groupKey)`
- `collapseGroup(groupKey)`
- `expandAllGroups()`
- `collapseAllGroups()`

Expansion snapshot roundtrip is deterministic for equal source/config.

## Selection And Mutation Semantics

- Selection operates on the current flattened projection.
- Selecting a tree group row selects that visible group row only; it does not implicitly select descendants.
- Shift selection spans visible flattened rows, including group rows when they are inside the visible range.
- Keyboard shift-extension and additive cell ranges preserve group rows as visible flattened rows in app selection state.
- Row selection may focus/select visible tree group row ids and reconciles against the current flattened projection.
- Expansion changes mark virtual selections stale; row selection reconciliation removes hidden descendant row ids while preserving visible tree group row ids.
- App clipboard copy/cut, paste targets, clear/delete, and fill source/target ranges over group rows are blocked unless a future server operation explicitly defines group-row export or mutation semantics.
- Server-backed tree/group placeholder rows follow the same group-row blocking rule; they must not fall back to local leaf-only mutation or clipboard reads.
- Local cell mutations never silently apply to only the leaf subset of a selected range that includes tree group rows.

## Diagnostics

Tree diagnostics are exposed via row model snapshot:

- `treeDataDiagnostics.orphans`
- `treeDataDiagnostics.cycles`
- `treeDataDiagnostics.duplicates`
- `treeDataDiagnostics.lastError`

Diagnostics must be deterministic and coalesced for repeated no-op refreshes.

## Failure Modes and Expected Behavior

Orphan rows:

- `root`: row appears at top level; diagnostics incremented.
- `drop`: row is excluded from visible projection.
- `error`: projection throws deterministic error.

Cycles:

- `ignore-edge`: offending edge ignored; projection continues.
- `error`: projection throws deterministic error.

Duplicate `rowId` (client row model):

- update is rejected,
- previous valid snapshot/revision is preserved,
- diagnostics include duplicate error context.

## Server/Data Source Notes

For `createDataSourceBackedRowModel`, tree operations forward explicit tree pull context:

- `treeData.operation`
- `treeData.scope` (`branch` or `all`)
- `treeData.groupKeys`

Backpressure semantics (abort/coalesce/defer) are identical to non-tree pulls and remain mandatory for CI.

## Examples

Path mode:

```ts
const rowModel = createClientRowModel({
  rows,
  initialTreeData: {
    mode: "path",
    expandedByDefault: false,
    filterMode: "include-parents",
    getDataPath: (row) => row.path,
  },
})
```

Parent mode:

```ts
const rowModel = createClientRowModel({
  rows,
  initialTreeData: {
    mode: "parent",
    orphanPolicy: "root",
    cyclePolicy: "ignore-edge",
    getParentId: (row) => row.parentId ?? null,
    rootParentId: null,
  },
})
```

## Migration: GroupBy-only -> TreeData

1. Keep `groupBy` for analytical grouping use-cases.
2. For intrinsic hierarchy, move to `initialTreeData`:
   - replace row-level hierarchy hacks in adapter/UI with row-model config.
3. Define stable `rowId` resolver if source rows do not carry one.
4. Choose explicit policies (`orphanPolicy`, `cyclePolicy`, `filterMode`) instead of relying on adapter behavior.
5. Re-run contracts and tree e2e/perf gates:
   - `pnpm run test:datagrid:tree:contracts`
   - `pnpm run bench:datagrid:tree:assert`

Behavioral difference to expect:

- `groupBy` groups by value snapshots,
- `treeData` preserves structural parent-child semantics and expansion identity by tree group keys.

## References

- `docs/datagrid-tree-data-behavior-matrix.md`
- `docs/datagrid-model-contracts.md`
- `docs/datagrid-data-source-protocol.md`
