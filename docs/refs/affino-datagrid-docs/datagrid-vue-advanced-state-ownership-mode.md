# DataGrid Vue Advanced State Ownership Mode

Updated: `2026-02-09`

## Decision

For `@affino/datagrid-vue/advanced`, we lock **Mode B** as default architecture rule:

- `core` owns canonical runtime state.
- `advanced` reads from core (or explicit refs/snapshots) and translates intent/state for UI.
- `advanced` may emit explicit commands/events, but does not become a second source of truth.

Mode A (`advanced` owns state and pushes to core) is reserved only for explicitly isolated adapters and must not be mixed inside the same composable.

## Hard Rules

1. One composable = one ownership mode.
2. No mixed A+B semantics in one composable.
3. Canonical data lifecycle stays in core models/services.
4. Advanced layer should expose pure orchestration helpers or deterministic bridges.
5. UI layer consumes advanced outputs, not core internals directly.

## Why

- prevents state drift and cyclic updates;
- keeps runtime deterministic under virtualization/selection/editing flows;
- makes orchestration reusable across Vue/Laravel/React adapters.

## Scope

Applies to:

- `@affino/datagrid-vue/advanced`
- orchestration wrappers in `@affino/datagrid-vue` that delegate to `@affino/datagrid-orchestration`

Related docs:

- `docs/datagrid-vue-advanced-entrypoint.md`
- `docs/datagrid-cross-platform-adapter-protocol.md`
- `docs/archive/datagrid/checklists/datagrid-vue-demo-first-refactor-pipeline-checklist.md`
