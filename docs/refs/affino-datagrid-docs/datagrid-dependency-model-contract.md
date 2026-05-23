# DataGrid Dependency Model Contract

Scope: `packages/datagrid-core/src/models/dependencyModel.ts`

## Goal

Fix a formal typed model for dependency graph nodes and edges so patch analysis and projection invalidation can scale without implicit string conventions.

## Node Domains

- `field:*` - structural data field paths (`field:service.region`).
- `computed:*` - derived/calculated tokens (`computed:latencyRank`).
- `meta:*` - UI/runtime meta tokens (`meta:rowColor`).

For formula/runtime integration, client row-model currently exposes stable formula meta keys:

- `meta:rowId`
- `meta:rowKey`
- `meta:sourceIndex`
- `meta:originalIndex`
- `meta:kind`
- `meta:isGroup`

## Canonical Types

- `DataGridFieldNode`
- `DataGridComputedNode`
- `DataGridMetaNode`
- `DataGridDependencyNode` (union)
- `DataGridDependencyEdge` (`structural|computed`)

## API Contract

- `normalizeDataGridDependencyToken(token, fallbackDomain?)`
  - always returns explicit-domain token (`field:*|computed:*|meta:*`).
  - throws on empty input or empty payload after explicit domain prefix.
- `parseDataGridDependencyNode(token, fallbackDomain?)`
  - returns typed node with domain-specific payload field (`path|name|key`).
- `createDataGridDependencyEdge({ sourceToken, targetToken, kind? })`
  - returns typed source/target nodes + edge kind.

## Unit Contracts

Contract tests:

- `packages/datagrid-core/src/models/__tests__/dependencyModel.spec.ts`
  - token normalization,
  - domain parsing,
  - typed edge creation,
  - guardrails for empty/invalid tokens.
