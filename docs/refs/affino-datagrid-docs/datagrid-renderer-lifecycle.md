# DataGrid Renderer Lifecycle

Scope: `@affino/datagrid-vue-app` column `cellRenderer` and `groupCellRenderer` callbacks.

## Ownership

- The grid owns the cell wrapper, row/column virtualization, pinned panes, selection, focus restoration, editing, clipboard, fill, row grouping state, and ARIA on the grid surface.
- A renderer owns only the Vue children it returns for that cell.
- Renderers run synchronously during the Vue render pass for the mounted table stage.
- Renderer callbacks are display hooks, not state transition hooks. State changes must flow through column contracts such as `cellInteraction`, editing, row model updates, grouping APIs, or host-owned application state.

## Context Contract

`DataGridAppCellRendererContext` includes:

- `row`, `rowNode`, `rowOffset`, `column`, and `columnIndex`
- `value`: the raw string value read by the stage
- `displayValue`: the formatted display string after presentation rules
- `surface.kind`: `"real"` for materialized rows and `"placeholder"` for visual placeholder rows
- `interactive`: the resolved `cellInteraction` invocation surface, or `null`

`DataGridAppGroupCellRendererContext` has the same display fields for group rows and adds `group`:

- `key`, `field`, `value`, and `childrenCount`
- `isLabelColumn`
- `renderMeta` with normalized group row metadata
- `toggle()`, the canonical expand/collapse trigger for that group row

## Lifecycle Guarantees

- A renderer may be invoked many times for the same logical cell.
- A rendered child may unmount and remount when rows or columns enter and leave the virtual window.
- Horizontal virtualization, vertical virtualization, pinned panes, grouping, filtering, sorting, and placeholder rows can all cause renderer output to be recreated. Active touch scroll keeps visible custom renderer output mounted instead of replacing it with display values.
- If a renderer returns `null` or `undefined`, the stage falls back to `displayValue`.
- If a renderer throws, the stage preserves the cell wrapper and falls back to `displayValue` for that cell.
- The grid does not preserve renderer-local component state across virtual unmounts. Persist durable state in row data, host state, or the row model.
- Vue components returned by a renderer own their own cleanup through normal Vue unmount hooks.

## Interaction Rules

- Keep the grid cell wrapper as the owner of grid focus, selection, editing, menus, clipboard, fill, and keyboard navigation.
- For action cells, declare `column.cellInteraction` and call `context.interactive?.activate(...)` from custom content.
- Do not mutate row data, selection, focus, editor state, row grouping, or grid runtime state while the renderer callback is running.
- Focusable children are allowed, but they must have accessible names and should stop pointer events only for their own action. They must not become a competing grid keyboard model.
- For group rows, call `context.group.toggle()` from an explicit disclosure/action inside `groupCellRenderer`; do not treat the whole cell body as the group toggle.

## Async And Cleanup

- Renderer callbacks are synchronous. Do not return promises from `cellRenderer` or `groupCellRenderer`.
- Load async data outside the renderer and pass resolved state through row data, column state, or host state.
- Render a stable placeholder while async data is pending.
- Components returned by renderers must release timers, observers, subscriptions, and DOM listeners on unmount.

## Performance Rules

- Keep renderer work proportional to one visible cell.
- Avoid synchronous layout reads such as `getBoundingClientRect()`, `clientWidth`, `offsetWidth`, `scrollHeight`, or computed-style reads inside renderer callbacks.
- Avoid reactive writes from renderer callbacks; they can amplify virtual-window updates.
- Keep expensive charts, popovers, observers, and network work behind explicit user intent.
- Use stable Vue keys for repeated child VNodes when child identity matters.
- Treat wide grids, pinned panes, and pinned-bottom rows as multipliers for renderer invocation cost.
- Renderer callback p95 duration is part of the enterprise browser-frame gate through `PERF_BUDGET_MAX_CELL_RENDERER_P95_MS` and `PERF_BUDGET_MAX_GROUP_CELL_RENDERER_P95_MS`.
- Mount/unmount churn while scrolling is reviewed beside renderer duration through `churnTelemetry` row/cell mount budgets.

## Validation

Focused package coverage lives in `packages/datagrid-vue-app/src/__tests__/DataGrid.contract.spec.ts` and covers:

- focusable renderer children with grid-owned cell semantics
- `interactive.activate(...)` routing through `cellInteraction`
- `groupCellRenderer` expansion via `group.toggle()`
- renderer output after virtual row/column remounts
- component cleanup when renderer children unmount

Run:

```bash
pnpm --filter @affino/datagrid-vue-app exec vitest run --config vitest.config.ts src/__tests__/DataGrid.contract.spec.ts
```
