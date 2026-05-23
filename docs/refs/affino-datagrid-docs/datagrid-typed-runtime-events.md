# DataGrid Typed Runtime Events

Updated: `2026-02-11`

Import path:
- `@affino/datagrid-core/advanced`

Related tier contract:
- `docs/datagrid-event-contract-tiers.md`

Runtime events in `@affino/datagrid-core` are now split into three explicit domains:

## Host Events

Host events are component-facing callbacks (`UiTableEventHandlers`) and remain source of truth for external integrations.

- Canonical map: `DataGridHostEventMap`
- Name union: `DataGridHostEventName`
- Name mapping for Vue emit: `HOST_EVENT_NAME_MAP`

## Plugin Events

Plugin bus is now typed and supports strict payloads.

- Base lifecycle events:
  - `runtime:initialized`
  - `runtime:disposing`
- Combined map contract:
  - `DataGridRuntimePluginEventMap<TCustomPluginEvents>`
  - includes host event bridge + runtime plugin lifecycle + custom plugin events

`DataGridRuntime` now exposes:

- `emitPlugin(event, ...args)`
- `onPlugin(event, handler)`

## Plugin usage (why + how)

Why this exists:

- add new cross‑cutting behaviors (context menu actions, analytics, custom row policies)
- keep Core deterministic while letting adapters extend behavior safely
- ship integrations without forking the runtime

How to use it (minimal pattern):

```ts
import type { DataGridEventMap } from "@affino/datagrid-plugins"
import type { DataGridRuntimePluginEventMap } from "@affino/datagrid-core/advanced"

type MyPluginEvents = DataGridEventMap & {
  "plugin:cell-audit": [payload: { rowKey: string; columnKey: string; value: unknown }]
}

type MyPluginBus = DataGridRuntimePluginEventMap<MyPluginEvents>

const runtime = createDataGridRuntime({ /* ... */ })

runtime.onPlugin("plugin:cell-audit", ({ rowKey, columnKey, value }) => {
  // analytics / logging / integration hook
})

runtime.emitPlugin("plugin:cell-audit", {
  rowKey: "row-1",
  columnKey: "owner",
  value: "NOC",
})
```

Notes:

- keep plugin events in adapters/orchestration layers, not in Core models
- prefer narrow payloads and explicit names (`plugin:*` prefix)

## Internal Runtime Events

Internal service/runtime signals are isolated from host/plugin domains:

- `lifecycle:init`
- `lifecycle:dispose`
- `host:dispatched`
- `plugin:host-unknown`
- `plugin:capability-denied`

Contract:

- `DataGridRuntimeInternalEventMap`
- `onInternalEvent(name, args)` in runtime options

Legacy compatibility:

- `onUnknownPluginEvent` kept as deprecated fallback bridge.

## Why this is prerequisite for GridApi

`GridApi` can now rely on:

- deterministic host dispatch ordering,
- strict plugin payload typing,
- explicit internal lifecycle signals.

This removes stringly-typed ambiguity before introducing semver-stable unified API facades.

## Tiered contract note

- Stable app-facing event contract is exported from `@affino/datagrid-core` via:
  - `DataGridStableEventMap`
  - `DataGridStableEventName`
  - `DataGridEventEnvelope`
- Advanced runtime/plugin details remain in `@affino/datagrid-core/advanced`.
