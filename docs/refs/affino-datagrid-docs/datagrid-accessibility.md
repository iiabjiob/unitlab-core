# DataGrid Accessibility

Updated: `2026-05-20`

This document is the current-state accessibility contract for the Affino DataGrid packages. It records implemented behavior, known gaps, and the validation expected for future enterprise accessibility slices.

## Ownership

- `packages/datagrid-core/src/a11y/headlessA11yStateMachine.ts` owns deterministic headless focus, keyboard, and ARIA state.
- `packages/datagrid-vue/src/adapters/a11yAttributesAdapter.ts` maps headless grid/cell ARIA state to DOM-ready attributes.
- `packages/datagrid-orchestration/src/accessibility/useDataGridA11yCellIds.ts` owns stable cell/header id and one-based ARIA index helpers.
- `packages/datagrid-vue-app/src/stage/*` owns the mounted virtualized stage, pinned panes, row index cells, editors, overlays, and focus restoration.
- `packages/datagrid-vue-app/src/host/DataGridDefaultRenderer.ts` owns app-level status regions rendered around the stage.

## Implemented Stage Contract

The mounted table stage currently exposes baseline ARIA metadata for the virtualized body:

- the center body viewport exposes `role="grid"`, `aria-rowcount`, `aria-colcount`, and `aria-multiselectable="true"`;
- rendered body rows expose `role="row"` and one-based `aria-rowindex`;
- rendered center body cells expose one-based `aria-rowindex` and `aria-colindex`;
- rendered pinned body cells use the same row indexes and logical column indexes as the center pane;
- normal body cells expose `role="gridcell"` unless an interactive role such as `checkbox` or `rowheader` overrides it;
- rendered selected cells expose deterministic `aria-selected` state after virtualized unmount/remount;
- leaf header cells expose `role="columnheader"`, one-based `aria-colindex`, sortable-column `aria-sort`, and contextual accessible names;
- leaf header and body cells expose deterministic sanitized DOM ids across center, pinned, and virtualized remount paths;
- header resize and text-filter controls include the target column in their accessible names;
- normal-mode keyboard tabbing exposes one stage owner: focused row index first, visible selection anchor cell second, and body viewport fallback only when no visible focus target exists;
- grouped rows expose `aria-expanded` and a group label summary under the current `grid` role model;
- placeholder rows expose row-level disabled/context metadata while cells preserve row/column coordinates;
- row-selection checkbox cells expose `role="checkbox"` and `aria-checked`;
- active inline text, date/datetime, and select editors expose accessible names with column and row context while preserving invalid and pending state;
- placeholder cells that cannot materialize into editable rows expose disabled state while preserving their row/column coordinates;
- decorative canvas chrome, selection overlays, fill overlays, and move overlays are hidden from assistive technologies;
- app status regions use polite live-region semantics for clipboard, edit, fill, range move, history, sort/filter, and row-model loading/error outcomes.

This stage contract is covered by component tests in `packages/datagrid-vue-app/src/__tests__/DataGrid.contract.spec.ts`, mounted-grid coverage in `e2e/sandbox-interactions.spec.ts`, and the focused large-grid A11Y browser benchmark `pnpm run bench:datagrid:enterprise:a11y:browser:assert`.

## Focus Model

The app stage currently uses stage-native DOM focus restoration and selection snapshot state as the mounted-grid owner. Normal browsing mode uses one tabbable stage target: focused row index, visible selection anchor cell, or body viewport fallback. The mounted stage keeps roving DOM focus and does not emit app-stage `aria-activedescendant`; stable cell ids are still rendered so browser tests and future relationships can resolve mounted cells deterministically.

Under the current roving-focus model:

- do not add app-stage `aria-activedescendant` without a deliberate focus-model change;
- preserve existing keyboard navigation, editor focus takeover, and focus restoration behavior;
- avoid adding additional tabbable descendants in body cells unless the interaction metadata explicitly requires it;
- treat any change to normal-mode tab stops as browser-visible behavior requiring component and Playwright validation.

## Known Gaps

- Pivot header group semantics and deeper menu relationship metadata still need browser-level validation.
- A future treegrid proposal would be required before adding hierarchy-only row metadata such as full tree levels/positions.
- Datasource loading/error placeholders need stronger per-row screen-reader context beyond the grid-level status message.
- Axe-style static checks and manual assistive-technology validation are still required before claiming enterprise screen-reader readiness.

## Validation Expectations

Runtime accessibility slices should include the smallest relevant validation first:

- component tests for rendered ARIA roles, counts, indexes, labels, focus ownership, and state;
- Playwright `@a11y` tests for browser-mounted behavior across scroll, virtualization remount, pinned panes, editing, grouped rows, and datasource placeholders;
- `pnpm run bench:datagrid:enterprise:a11y:browser:assert` for large-grid scroll paths when adding ARIA state to hot render surfaces;
- manual screen-reader smoke testing before claiming WCAG conformance or enterprise screen-reader readiness.

Do not document WCAG conformance until browser and assistive-technology validation exists for the mounted app stage.
