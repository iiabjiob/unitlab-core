# DataGrid Config Decomposition

Updated: `2026-02-07`

`tableConfig` normalization is now split into explicit domains:

- `data`
- `model`
- `view`
- `interaction`

## Entry Points

From `packages/datagrid-core/src/config/tableConfig.ts`:

- `migrateLegacyUiTableConfig(raw)`
- `normalizeTableDataSection(config)`
- `normalizeTableModelSection(config)`
- `normalizeTableViewSection(config)`
- `normalizeTableInteractionSection(config)`
- `normalizeTableConfigSections(raw)`
- `normalizeTableProps(raw)` (final compatibility shape for Vue layer)

## Migration Adapter

`migrateLegacyUiTableConfig` maps legacy flat props into canonical config sections:

- `rows/totalRows/summaryRow` -> `config.data`
- `columns/columnGroups` -> `config.columns`
- appearance fields -> `config.appearance`
- selection + metrics -> `config.selection`/`config.features`
- load/runtime fields -> `config.load`
- debug/state fields -> `config.debug`/`config.state`
- `events/plugins` -> `config.events`/`config.plugins`

Legacy top-level values retain precedence when provided.

## Invariants

- No cross-domain mixed normalization path inside one function.
- `normalizeTableProps` is now pure composition of normalized sections.
- Existing downstream contract (`NormalizedTableProps`) is preserved for adapters/components.
