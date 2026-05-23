# DataGrid Plugin Capability Model

Updated: `2026-05-20`

This document defines the capability boundary for runtime plugins in `@affino/datagrid-plugins` (consumed by `@affino/datagrid-core`).

For the cross-package plugin lifecycle decision, see `docs/datagrid-plugin-lifecycle.md`. The stable public plugin facade is `DataGridApi.plugins`; this capability model is the advanced lower-level runtime for host/plugin environments that need explicit capability negotiation.

## Policy

- Plugins do not receive direct host/runtime internals.
- Plugins can access only explicitly declared capabilities.
- Host provides capabilities through a capability map.
- Undeclared or missing capabilities are denied fail-fast.

## Contracts

Plugin setup context (`DataGridPluginSetupContext`) exposes:

- `hasCapability(name)` - side-effect free availability check.
- `requestCapability(name)` - returns function or `null`.
- `invokeCapability(name, ...args)` - executes capability or throws on denied access.

Plugin declaration (`DataGridPlugin`) adds:

- `capabilities?: string[]` - explicit allowlist per plugin.

## Runtime Integration

Runtime plugin context now requires:

- `getCapabilityMap(): Record<string, (...args) => unknown>`

Runtime emits internal denial diagnostics:

- `plugin:capability-denied` with `{ pluginId, capability, reason }`
- `reason` is `not-declared` or `not-provided`

## Why

- Prevents plugins from bypassing public contracts through broad host-expose objects.
- Keeps plugin interactions auditable and semver-safe.
- Makes cross-platform adapters deterministic and headless-friendly.

## Contract Tests

- `packages/datagrid-core/src/runtime/__tests__/dataGridRuntime.events.contract.spec.ts`
- `packages/datagrid-core/src/adapters/__tests__/adapterRuntimeProtocol.contract.spec.ts`
