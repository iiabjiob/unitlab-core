# DataGrid Orchestration Public Contract

Decision date: `2026-05-20`

Scope: `@affino/datagrid-orchestration`

## Tier Decision

The package root export is classified as `advanced-adapter-internal`.

The root remains a package export for existing adapter and framework integration code, but it is not an app-facing stable API. Its compatibility promise is narrower than the stable roots of `@affino/datagrid-core`, `@affino/datagrid-vue`, and `@affino/datagrid-vue-app`.

## Intended Consumers

- Affino-maintained adapters and app packages, including `@affino/datagrid-vue` and `@affino/datagrid-vue-app`.
- Vetted advanced adapter authors who are building framework bindings and can track low-level interaction lifecycle changes.

External product and app integrations should prefer:

- `@affino/datagrid-core` for stable framework-agnostic model, datasource, and API contracts.
- `@affino/datagrid-vue` or `@affino/datagrid-vue/stable` for Vue adapter integration.
- `@affino/datagrid-vue-app` for mounted app/component integration.
- `DataGridApi`, `api.plugins`, documented Vue props/events, and app-level renderer hooks for extensibility.

## Contract Rules

- Do not use deep imports from `@affino/datagrid-orchestration/src/*` or generated `dist/*` files.
- Treat root exports as low-level interaction primitives for adapter composition, not product-level extension APIs.
- Review changes through the public API inventory and declaration report before refreshing baselines.
- Pair intentional public type or behavior changes with migration notes when advanced adapter consumers must change code.
- Do not promote an orchestration primitive to stable ecosystem support through the broad root alone.

## Expansion Rule

If an orchestration primitive needs stable ecosystem support, propose a focused public API change first. The proposal should name the intended tier, export path, migration notes, tests, and compatibility expectations before changing the package export map.

Recommended future shape is a dedicated tiered entrypoint, for example a stable or advanced subpath, instead of widening the meaning of the current root.

## Validation

Run these checks when the orchestration public surface or classification changes:

```bash
pnpm run quality:api:datagrid:inventory
pnpm run quality:api:datagrid:report
pnpm --filter @affino/datagrid-orchestration type-check:public
pnpm --filter @affino/datagrid-orchestration test:contracts
```
