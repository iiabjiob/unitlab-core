# DataGrid Event Matrix

Scope: public and integration event surfaces across `@affino/datagrid-core`, `@affino/datagrid-vue`, and `@affino/datagrid-vue-app`.

## Preferred Integration Path

- Use `api.events` for stable public runtime observation.
- Use `DataGrid` Vue emits when integrating through the app component.
- Use `api.plugins.onEvent` for public plugin observation through the stable plugin facade.
- Use `createGrid().on/emit` only for local Vue feature composition; it is not the canonical public runtime event contract.
- Do not listen to both `api.events` and mirrored Vue emits for the same workflow unless the integration needs both headless and component-level lifecycles.

## Event Surface Matrix

| Surface | Event names | Payload owner | Ordering | Error handling | Preferred use |
| --- | --- | --- | --- | --- | --- |
| `api.events` | `rows:changed`, `columns:changed`, `projection:recomputed`, `selection:changed`, `row-selection:changed`, `pivot:changed`, `transaction:changed`, `viewport:changed`, `state:import:begin`, `state:imported`, `state:import:end`, `error` | `@affino/datagrid-core` typed event map | Deterministic in-process ordering; reentrant emissions are queued FIFO | Listener exceptions are not swallowed; integration listeners must isolate their own failures | Stable runtime observation, headless integrations, diagnostics, plugin input |
| `api.plugins.onEvent` | Same event names as `api.events` | `@affino/datagrid-core` typed event map | Delivered from the core event stream after plugin registration | Plugin handler failures are isolated from core event dispatch and other plugins | Stable public plugin observation |
| `DataGrid` emits | `cell-change`, `cell-edit`, `selection-change`, `row-selection-change`, `update:*`, `toolbar-modules-change`, `ready` | `@affino/datagrid-vue-app` component facade | Mirrors runtime host events and controlled-prop snapshots in component lifecycle order | Vue listener behavior applies; handler failures are host-owned | Component consumers and controlled Vue props |
| `createGrid` feature bus | String event names chosen by local features | Local Vue feature | Synchronous registration-order delivery | Handler exceptions propagate and stop the current local emit | Local feature coordination only |
| Internal runtime host | `cell-change`, `selection-change`, `row-selection-change` | `@affino/datagrid-vue-app` runtime host | Bridges selected `api.events` into app component emits | Internal component boundary | Do not consume directly outside package internals |

## Core API Event Ordering

`api.events` is synchronous and ordered inside one runtime tick.

Row-model subscription ticks emit in this order when each condition applies:

1. `rows:changed`
2. `projection:recomputed`
3. `pivot:changed`
4. `viewport:changed`

`api.rows.batch(...)` coalesces event payloads by event name and flushes in the documented core order:

1. `state:import:begin`
2. `rows:changed`
3. `columns:changed`
4. `projection:recomputed`
5. `pivot:changed`
6. `selection:changed`
7. `row-selection:changed`
8. `transaction:changed`
9. `viewport:changed`
10. `state:imported`
11. `state:import:end`
12. `error` events, in queued order

`api.state.set(...)` is a logical import boundary:

1. `state:import:begin`
2. runtime events emitted while state is applied
3. `state:imported` after successful apply
4. `state:import:end`

If an event listener mutates the grid and causes another event, the nested event is queued FIFO behind the currently dispatched event. This preserves deterministic reentrant delivery without making state import atomic.

## Vue App Event Mapping

| Core/runtime event | Vue app emit | Notes |
| --- | --- | --- |
| `rows:changed` | `cell-change` | Emitted by `DataGridRuntimeHost`; app facade then emits controlled state snapshots when relevant. |
| Stage edit commit | `cell-edit` | Emitted after the edit patch has been applied. A `cell-change` from the row mutation may already have fired. |
| `selection:changed` | `selection-change` | Selection-only changes do not emit `update:state`. |
| `row-selection:changed` | `row-selection-change` | Carries `{ snapshot }`. |
| Row-selection watcher | `update:rowSelectionState` | Controlled row-selection snapshots are emitted through `update:rowSelectionState`; typed changes use `row-selection-change`. |
| Controlled state snapshots | `update:state`, `update:columnState`, `update:columnOrder`, `update:hiddenColumnKeys`, `update:columnWidths`, `update:columnPins`, `update:groupBy`, `update:viewMode` | Use these for Vue controlled-prop synchronization. |
| Runtime ready | `ready` | Provides `{ api, rowModel }` after the runtime is available. |

## Reentrancy And Failure Rules

- `api.events` listeners run in process and synchronously. Listener exceptions are integration failures and are not converted to `error` events.
- `api.events.error` is for recoverable runtime conflicts and protocol errors, not listener exceptions.
- `api.plugins.onEvent` failures are isolated from core dispatch and other plugin handlers.
- `createGrid` feature-bus handlers are local and synchronous. Handler failures propagate to the caller of `emit`.
- Vue app emits follow Vue listener semantics. Host applications should isolate failures in their own event handlers.

## Validation

Focused coverage lives in:

- `packages/datagrid-core/src/core/__tests__/gridApi.contract.spec.ts`
- `packages/datagrid-vue/src/grid/__tests__/eventBus.contract.spec.ts`
- `packages/datagrid-vue-app/src/__tests__/DataGrid.contract.spec.ts`

Run:

```bash
pnpm --filter @affino/datagrid-core test:contracts
pnpm --filter @affino/datagrid-vue test:contracts
pnpm --filter @affino/datagrid-vue-app exec vitest run --config vitest.config.ts src/__tests__/DataGrid.contract.spec.ts
```
