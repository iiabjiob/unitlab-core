# DataGrid Legacy Compatibility Window

Updated: `2026-02-07`

## Scope

Status: **closed early by explicit owner decision** (`2026-02-07`).

Compatibility shim was introduced, then removed in the same migration wave because this codebase has a single integrator and no external consumers.

Removed legacy viewport APIs:

- `controller.setProcessedRows(rows)` (removed)
- `controller.setServerIntegration({ enabled, rowModel })` (removed)
- `controller.options.serverIntegration` (removed)

## Runtime Behavior

- Canonical contract is model-driven only:
  - `setRowModel(...)`
  - `setColumnModel(...)`
- Adapter-level data normalization remains in Vue composables.

## Timeline

- Announcement: `2026-02-07`
- Effective removal: `2026-02-07`

## Migration Targets

- `setProcessedRows(rows)` -> `setRowModel(createClientRowModel({ rows }))`
- `setServerIntegration(...)` -> `setRowModel(createServerBackedRowModel({ source }))`
