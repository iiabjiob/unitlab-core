# DataGrid Core Advanced Reference

Updated: `2026-03-03`

This is the canonical reference for `@affino/datagrid-core/advanced`.
Use it only when stable root APIs are not enough.

## Advanced constructors

| Factory | Returns | Typical use |
| --- | --- | --- |
| `createDataGridRuntime(options)` | `DataGridRuntime` | Typed host/plugin runtime events and lifecycle hooks. |
| `createDataGridAdapterRuntime(options)` | `DataGridAdapterRuntime` | Adapter event-name mapping (`vue/react/laravel/web-component`). |
| `createDataGridTransactionService(options)` | `DataGridTransactionService` | Undo/redo + rollback-safe command execution. |
| `createDataGridA11yStateMachine(options)` | `DataGridA11yStateMachine` | Headless focus/keyboard/ARIA state machine. |
| `createDataGridViewportController(options)` | `DataGridViewportController` | Virtual window, pinned geometry, integration snapshots. |

## Advanced utility groups

`@affino/datagrid-core/advanced` also exports:

- selection geometry helpers (`createGridSelectionRange`, `clampGridSelectionPoint`, ...)
- event protocol helpers (`createDataGridEventEnvelope`, `DATAGRID_EVENT_TIERS`, ...)
- public protocol codemod helper (`transformDataGridPublicProtocolSource`)

## When to stay on stable root

Prefer `@affino/datagrid-core` if you only need:

- row/column models
- `DataGridApi` facade
- standard sort/filter/group/pivot/selection/editing flows
- default client integrations

Switch to `advanced` only for explicit runtime/transport/layout internals.

## Related references

- `docs/datagrid-core-factories-reference.md`
- `docs/datagrid-grid-api.md`
- `docs/datagrid-feature-catalog.md`
