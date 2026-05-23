# DataGrid Versioned Public Protocol

Updated: `2026-05-20`

This document defines semver-safe public protocol rules for:

- `@affino/datagrid-core`
- `@affino/datagrid-vue`

Canonical protocol source:
- `packages/datagrid-core/src/protocol/versionedPublicProtocol.ts`

## Public Tiers

Tier 1 (stable, semver-safe):
- `@affino/datagrid-core`
- `@affino/datagrid-vue`

Tier 2 (advanced, power-user APIs):
- `@affino/datagrid-core/advanced`

Tier 3 (internal, unsafe):
- `@affino/datagrid-core/internal`

Event tier contract (stable export):
- `DATAGRID_EVENT_TIERS`
- `DATAGRID_EVENT_TIER_ENTRYPOINTS`
- `DataGridStableEventMap`
- `DataGridEventEnvelope`

## Semver Rules

Stable entrypoints are semver-safe and guaranteed.
Advanced entrypoints are supported but may evolve with tighter deprecation windows.
Internal entrypoints are explicitly unsafe and have no semver guarantees.

Forbidden public integration paths:
- `@affino/datagrid-core/src/*`
- `@affino/datagrid-core/viewport/*`
- `@affino/datagrid-vue/src/*`

Rule: if an import path is outside tiered entrypoints above, it is not part of public contract.

`@affino/datagrid-core` enforces this through its package export map: only `.`, `./advanced`, and `./internal` are exported. Source-shaped wildcard paths are intentionally not exported.

## Deprecation Windows

Current deprecations are versioned in `DATAGRID_DEPRECATION_WINDOWS`.

Facade-level deprecation planning is tracked in:
- `docs/quality/datagrid-facade-deprecation-plan.json`

Required fields per deprecation:
- `deprecatedIn`
- `removeIn`
- `replacement`
- `codemodCommand`

Status resolution:
- `active`: current package version `< deprecatedIn`
- `warning`: `>= deprecatedIn` and `< removeIn`
- `removal-ready`: `>= removeIn`

## Codemod for Breaking Changes

Codemod command:
- `pnpm run codemod:datagrid:public-protocol -- --write <path>`

Script:
- `scripts/codemods/datagrid-public-protocol-codemod.mjs`

Current transformations:
- deep import rewrite to tiered public entrypoints
- `createTableViewportController` -> `createDataGridViewportController`
- root import rewrite for advanced symbols (`@affino/datagrid-core` -> `@affino/datagrid-core/advanced`)
- migration TODO marker for `serverIntegration:` blocks

Run in dry mode:
- `pnpm run codemod:datagrid:public-protocol -- <path>`

## Contract Tests

- `packages/datagrid-core/src/protocol/__tests__/versionedPublicProtocol.contract.spec.ts`
- `packages/datagrid-core/src/protocol/__tests__/publicProtocolCodemod.contract.spec.ts`
- `packages/datagrid-core/src/protocol/__tests__/entrypointTiers.contract.spec.ts`
