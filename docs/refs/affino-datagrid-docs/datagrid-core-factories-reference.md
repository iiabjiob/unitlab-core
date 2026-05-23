# DataGrid Core Factories Reference

Updated: `2026-03-03`

This is the constructor-level reference for `@affino/datagrid-core` stable entrypoint.

## Stable constructors

| Factory | Returns | Typical use |
| --- | --- | --- |
| `createDataGridCore(options?)` | `DataGridCore` | Service registry and lifecycle wiring. |
| `createDataGridApi(options)` | `DataGridApi` | Stable namespace facade (`lifecycle/rows/data/columns/view/pivot/selection/transaction/state/events/meta/policy/compute/diagnostics/plugins`). |
| `createClientRowModel(options?)` | `ClientRowModel<T>` | In-memory projection (filter/sort/group/pivot/aggregate/paginate). |
| `createDataGridColumnModel(options?)` | `DataGridColumnModel` | Column definitions/order/visibility/width/pin state. |
| `createDataGridEditModel(options?)` | `DataGridEditModel` | Headless edit patches and revision tracking. |
| `createDataGridSelectionSummary(options)` | `DataGridSelectionSummarySnapshot` | Deterministic summary over selected scope. |

## Additional stable constructors

| Factory | Returns | Typical use |
| --- | --- | --- |
| `createServerRowModel(options, config?)` | `ServerRowModel<T>` | Block cache + lazy-loading model. |
| `createServerBackedRowModel(options)` | `ServerBackedRowModel<T>` | Adapter from server row model to `DataGridRowModel`. |
| `createDataSourceBackedRowModel(options)` | `DataSourceBackedRowModel<T>` | Pull/push/invalidation/backpressure protocol model. |
| `createDataGridServerPivotRowId(input)` | `string` | Stable row-id generation for server-side pivot rows. |

## Canonical API assembly

Use `createDataGridCore(...)` as canonical assembly path:

```ts
import {
  createClientRowModel,
  createDataGridApi,
  createDataGridColumnModel,
  createDataGridCore,
} from "@affino/datagrid-core"

const rowModel = createClientRowModel({ rows })
const columnModel = createDataGridColumnModel({ columns })

const core = createDataGridCore({
  services: {
    rowModel: { name: "rowModel", model: rowModel },
    columnModel: { name: "columnModel", model: columnModel },
  },
})

const api = createDataGridApi({ core })
await api.start()
```

## `createDataGridApi(...)` input shapes

Supported options:

- `createDataGridApi({ core, plugins? })`
- `createDataGridApi({ lifecycle, init, start, stop, dispose, rowModel, columnModel, ... })`

For product integrations prefer `core` form.

## Related references

- `docs/datagrid-grid-api.md`
- `docs/datagrid-core-advanced-reference.md`
- `docs/datagrid-state-events-compute-diagnostics.md`
