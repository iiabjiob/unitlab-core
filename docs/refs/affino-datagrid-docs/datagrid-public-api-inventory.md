# DataGrid Public API Inventory

Updated: `2026-05-20`

This document is the current public-surface inventory for DataGrid packages. It classifies package export maps by tier and names the remaining boundary risks before API enterprise hardening changes the public contract.

The generated snapshot lives at `docs/quality/datagrid-public-api-inventory.json` and is checked by:

```bash
pnpm run quality:api:datagrid:inventory
```

The declaration-level API report lives at `docs/quality/datagrid-api-report.json` and is checked by:

```bash
pnpm run quality:api:datagrid:report
```

The API report reads emitted declarations from package `dist` folders. Run the relevant package builds first when declarations are missing.

## Package Export Tiers

| Package | Export paths | Current tier decision | Remaining risk |
| --- | --- | --- | --- |
| `@affino/datagrid-core` | `.`, `./advanced`, `./internal` | Root is stable, `advanced` is power-user, and `internal` is unsafe. Source-shaped wildcard exports are blocked. | New deep-import requirements must be added through an approved tiered entrypoint instead of `./*`. |
| `@affino/datagrid-vue` | `.`, `./stable`, `./app`, `./app/worker`, `./advanced/*`, `./worker` | Root and `./stable` are contract-equivalent stable entrypoints; app, worker, and advanced subpaths are advanced integration surfaces. | Keep root/stable equivalence and low-level advanced-hook exclusions covered by `entrypointTiers.contract.spec.ts`. |
| `@affino/datagrid-vue-app` | `.`, feature subpaths, `./internal` | Root and feature subpaths are app-facing stable surfaces; `./internal` is unsafe/internal. | Props, exposed Vue ref helpers, services, startup order, and renderer hooks need a public-vs-advanced table. |
| `@affino/datagrid-orchestration` | `.` | Root is advanced adapter-internal. It is exported for adapter/framework integration, not app-level stable API. | Future stable ecosystem needs require a public API proposal, tiered entrypoint, and migration notes before export-map changes. |
| `@affino/datagrid-server-adapters` | `.` | Stable backend adapter surface. | Compatibility notes should stay aligned with server datasource protocol changes. |
| `@affino/datagrid-server-client` | `.` | Stable backend client helper surface. | Live update, retry, and consistency-token changes need migration notes when public behavior changes. |

## Snapshot Policy

- Any new package export path must be classified in `scripts/check-datagrid-public-api-inventory.mjs`.
- Any changed source entrypoint or export declaration must refresh `docs/quality/datagrid-public-api-inventory.json` with `node ./scripts/check-datagrid-public-api-inventory.mjs --write-baseline` after review.
- Public API movement between stable, advanced, and internal tiers requires migration notes in `docs/datagrid-migration-guide.md` or the domain-specific guide.
- The generated inventory is an export-map and source-entrypoint snapshot.
- The generated API report is the declaration-level gate. Public type changes must refresh `docs/quality/datagrid-api-report.json` with `node ./scripts/check-datagrid-api-report.mjs --write-baseline` after semver review.

## Current Hardening Order

1. Keep orchestration stable-surface proposals explicit before export-map changes.
2. Keep declaration report changes paired with migration notes when public behavior or types change.
3. Refresh inventory and API report baselines only after reviewing public API impact.
