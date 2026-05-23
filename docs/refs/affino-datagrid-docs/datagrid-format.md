# DataGrid Format

`@affino/datagrid-format` is the shared headless formatting engine for DataGrid display values.

## Why It Exists

Formatting is part of presentation, not the row model itself.

The grid should continue to own raw values:

- formulas operate on raw values
- edits patch raw values
- clipboard/export can choose raw or formatted output explicitly

The format engine only turns visible raw values into readable strings for renderers.

## Current Scope

- `numberFormat`
- `dateTimeFormat`
- `Intl` formatter caching
- readable defaults for `date` and `datetime` columns

## Column Usage

```ts
{
  key: "amount",
  dataType: "currency",
  presentation: {
    numberFormat: {
      locale: "en-GB",
      style: "currency",
      currency: "GBP",
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    },
  },
}
```

```ts
{
  key: "start",
  dataType: "date",
  presentation: {
    dateTimeFormat: {
      locale: "en-GB",
      year: "numeric",
      month: "short",
      day: "2-digit",
      timeZone: "UTC",
    },
  },
}
```

## Principles

- Grid data stays raw
- formatting is display-only
- formatters are cached
- adapters format only the cells they render
- the implementation must stay framework-agnostic

## Adapter Contract

Adapters should call the format engine in their display path, not in the data pipeline.

This keeps formatting:

- cheap for virtualized grids
- portable across Vue and Laravel
- independent from editing and mutation semantics
