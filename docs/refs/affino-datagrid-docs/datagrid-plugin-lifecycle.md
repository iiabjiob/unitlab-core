# DataGrid Plugin Lifecycle

Updated: `2026-05-20`

This document defines the current canonical plugin model for DataGrid integrations. It does not introduce a new plugin system; it classifies the three existing extension shapes and their supported roles.

## Canonical Model

`api.plugins` on `DataGridApi` is the stable public plugin facade.

Use it when an extension needs to:

- register a public plugin id;
- observe typed `DataGridApi` events through `onEvent`;
- perform mutations only through the public `DataGridApi` facade;
- use stable register, unregister, list, and clear lifecycle calls.

`DataGridApiPluginDefinition` is intentionally small: `id`, `onRegister`, `onDispose`, and `onEvent`.

## Lifecycle Guarantees

- Plugin ids are trimmed and must be non-empty.
- Duplicate ids are rejected and do not replace the existing plugin.
- `onRegister` runs before the plugin is committed to the registry; if it throws, registration returns `false`.
- `onEvent` failures are isolated from core event dispatch and from other plugins.
- `onDispose` failures are isolated from unregister, clear, and API dispose paths.
- Event payloads delivered to plugins are shallow snapshots so handlers cannot mutate the dispatch payload shared by core.
- Plugins do not receive core runtime internals through `api.plugins`; state changes must go through public API namespaces.

## Advanced Capability Runtime

`@affino/datagrid-plugins` is the advanced capability-gated runtime foundation. Use it for host/adapter/plugin environments that need:

- declared capability allowlists;
- `hasCapability`, `requestCapability`, and `invokeCapability`;
- plugin-local events;
- setup cleanup registration;
- host-provided capability denial diagnostics.

This model is not a replacement for `api.plugins`. It is the lower-level foundation for integrations that need explicit capability negotiation before they bridge into host behavior.

## Vue Feature Layer

Vue `createGrid(...).use(feature)` features are local composition features for Vue wrappers.

Use them when a wrapper wants to:

- compose local runtime helpers;
- declare feature dependencies with `requires`;
- use the `createGrid` local event bus;
- clean up when the Vue runtime stops or unmounts.

Vue features are not the cross-package public plugin lifecycle. If a feature needs public plugin identity or API event observation, bridge it through `api.plugins`.

## Bridge Rules

- Public extensions should prefer `api.plugins`.
- Capability-sensitive extensions may use `@affino/datagrid-plugins` internally and expose a stable `api.plugins` registration point when they need public API identity.
- Vue-only composition should stay in `createGrid` features unless it needs cross-package plugin identity.
- Do not introduce another plugin abstraction without replacing or formally deprecating one of the existing roles.

## Validation

- Stable API plugin contracts: `packages/datagrid-core/src/core/__tests__/gridApi.contract.spec.ts`
- Capability runtime contracts: `packages/datagrid-core/src/runtime/__tests__/dataGridRuntime.events.contract.spec.ts`
- Adapter capability boundary contracts: `packages/datagrid-core/src/adapters/__tests__/adapterRuntimeProtocol.contract.spec.ts`
