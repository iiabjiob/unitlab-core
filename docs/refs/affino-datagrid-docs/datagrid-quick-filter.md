# DataGrid Quick Filter

Updated: `2026-05-09`

Quick filter is the grid-wide text filter contract for Affino DataGrid. It is part of the existing filter model, not a separate global search service.

## Contract

Use `DataGridFilterSnapshot.quickFilter`:

```ts
const filterModel = {
  columnFilters: {},
  advancedFilters: {},
  advancedExpression: null,
  quickFilter: {
    query: "platform eu",
    columns: ["service", "owner", "region"],
    mode: "tokens",
  },
} satisfies DataGridFilterSnapshot
```

Fields:

- `query`: trimmed text query. Empty queries are normalized away.
- `columns`: optional explicit searchable column keys. When omitted, client row-model helpers use the resolved searchable grid columns where the integration supplies them.
- `mode`: `contains` by default, or `tokens` when every whitespace token must match across the searched column values.

Do not put functions, accessors, or UI objects in the snapshot. The filter model must stay structured-clone compatible for worker-owned row models and serializable for server/data-source protocols.

## Projection Order

Quick filter runs inside the normal filter stage:

```text
filter -> sort -> group -> pivot -> aggregate -> paginate -> visible
```

It composes with column filters, style filters, and advanced filter expressions through `AND`.

Changing `quickFilter.query`, `quickFilter.columns`, or `quickFilter.mode` invalidates the filter stage and all downstream projection stages. It must not add a new projection stage.

## Searchable Columns

Default behavior is column-driven:

- search only grid columns, not arbitrary raw row object keys;
- prefer visible, non-system grid columns when a facade resolves the default searchable set;
- `column.capabilities.searchable === false` opts a column out;
- `column.capabilities.searchable === true` opts a column in even when it is not otherwise filterable;
- `filterable` is not required for quick filter.

When the filter snapshot provides `quickFilter.columns`, those keys are authoritative for that snapshot.

## Effective Values

Quick filter uses the same filter value resolution path as column filters:

- `readFilterCell(rowNode, columnKey)` when provided;
- otherwise the column field/value resolution path;
- never direct full-row object scanning.

This keeps formula-backed, derived, or display-label columns aligned with the value the user sees.

## Server/Data Source

`DataGridDataSourcePullRequest.filterModel` carries `quickFilter` inside the existing filter model. Do not add a top-level `search` field for row pulls.

Server-backed integrations must interpret `quickFilter` themselves:

- apply the query before server-side sort/group/pivot/pagination;
- use the supplied `quickFilter.columns` when present;
- keep the pull `reason` as `filter-change` for quick filter updates;
- include `quickFilter` in cache keys, projection signatures, and request serialization.

Column histogram `options.search` remains histogram-specific search for value-list UI. It is not a replacement for row-model quick filter.

## Vue/App Binding

The public app/facade path stays `filterModel`-first. Consumers can control quick filter by passing a controlled `filterModel` that contains `quickFilter`.

The Vue app shell also exposes a declarative convenience control:

```vue
<DataGrid
  :rows="rows"
  :columns="columns"
  quick-filter
  advanced-filter
/>
```

Object form:

```vue
<DataGrid
  :rows="rows"
  :columns="columns"
  :quick-filter="{
    placeholder: 'Search accounts',
    columns: ['service', 'owner', 'region'],
    mode: 'tokens',
  }"
/>
```

This prop is only shell UI over `filterModel.quickFilter`. It must not create a new state channel, `update:quickFilter` event, global service, or core UI dependency. Controlled consumers can continue to manage `filterModel.quickFilter` directly.
