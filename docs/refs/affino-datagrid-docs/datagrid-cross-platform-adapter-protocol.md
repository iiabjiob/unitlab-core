# DataGrid Cross-Platform Adapter Protocol

Updated: `2026-02-08`

`@affino/datagrid-core` now defines a single runtime adapter protocol for:

- Vue
- React
- Laravel
- Web Components

Import path:
- `@affino/datagrid-core/advanced`

## Public API

Source: `packages/datagrid-core/src/adapters/adapterRuntimeProtocol.ts`

Exports:

- `createDataGridAdapterRuntime(...)`
- `resolveDataGridAdapterEventName(kind, hostEvent)`
- `DataGridAdapterKind`
- `DataGridAdapterRuntime`

## Event Mapping Contract

Default host-event mapping:

- `vue` -> kebab-case (`reachBottom -> reach-bottom`)
- `laravel` -> kebab-case
- `web-component` -> kebab-case
- `react` -> host event name (`reachBottom`)

Adapters can override mapping through `mapHostEventName`.

## Runtime Capability Contract

`DataGridAdapterRuntime` keeps one semver-safe runtime surface across adapter targets:

- `emit` / `emitHost`
- `emitPlugin` / `onPlugin`
- `setHostHandlers`
- `setPlugins`
- `dispose`
- `kind`
- `mapHostEventName`

Plugin host bridge is capability-based:

- adapters provide `pluginContext.getCapabilityMap()`
- plugins can use only declared capabilities
- denied capability access is observable via runtime internal event `plugin:capability-denied`

Reference:
- `docs/datagrid-plugin-capability-model.md`

## Thin Adapter Contract

Adapters must stay thin and deterministic:

- Vue adapter:
  - reactivity/lifecycle/DOM bridge only;
  - delegates orchestration logic to shared core (`@affino/datagrid-orchestration`).
- Laravel/Livewire adapter:
  - hydration/event bridge only;
  - no duplicate runtime business logic in Blade/Livewire hooks.
- React adapter:
  - hooks/event bridge only;
  - no framework-local forks of selection/fill/clipboard orchestration.

Shared rule:

- behavior logic lives in `@affino/datagrid-orchestration` (and core models/services);
- adapters map host events and render concerns, not business state ownership.

## Compatibility Tests

Contract tests:

- `packages/datagrid-core/src/adapters/__tests__/adapterRuntimeProtocol.contract.spec.ts`

They validate:

- deterministic event mapping per adapter kind
- dispatch payload compatibility for different targets
- stable runtime capabilities (`setHostHandlers`, `setPlugins`, `dispose`)
