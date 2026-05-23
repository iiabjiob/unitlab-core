# DataGrid Event Contract Tiers

Updated: `2026-02-11`

This document defines the canonical event contract layers for `@affino/datagrid-core`.

Import path for tier contracts:
- `@affino/datagrid-core`

## Tiers

- `stable`: semver-safe consumer-facing events.
- `advanced`: power-user/runtime plugin bus events.
- `internal`: runtime lifecycle and diagnostics signals (unsafe for product integrations).

Canonical constants:
- `DATAGRID_EVENT_TIERS`
- `DATAGRID_EVENT_TIER_ENTRYPOINTS`

## Why this exists

The goal is to keep a strict and evolvable contract:
- avoid stringly-typed ad hoc events,
- avoid overgrowing into an unstructured AG Grid-style event list,
- keep stable vs advanced/internal boundaries explicit.

## Stable contract

Stable event names/payloads are derived from `DataGridEventHandlers` and should be treated as the primary app-integration layer.

Types:
- `DataGridStableEventMap<TData>`
- `DataGridStableEventName<TData>`

## Advanced contract

Advanced layer exposes runtime/plugin event maps for power users:
- `DataGridAdvancedEventMap<TPluginEvents>`
- `DataGridAdvancedEventName<TPluginEvents>`

This includes:
- host event bridge,
- runtime plugin lifecycle events,
- plugin-defined custom events.

## Internal contract

Internal layer contains runtime lifecycle/diagnostics signals:
- `DataGridInternalEventMap`
- `DataGridInternalEventName`

This tier is intentionally unsafe and may change without stable guarantees.

## Event envelope

For telemetry, adapter bridges, and cross-runtime routing, use:
- `DataGridEventEnvelope`
- `createDataGridEventEnvelope(...)`
- `isDataGridEventTier(...)`

Envelope fields:
- `tier`
- `name`
- `args`
- `source`
- `phase`
- `reason?`
- `affected?`
- `timestampMs`

`timestampMs` is auto-filled by `createDataGridEventEnvelope` when omitted.
