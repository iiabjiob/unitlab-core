# DataGrid Headless A11y Contract

Updated: `2026-05-18`

`@affino/datagrid-core` now includes a deterministic headless accessibility state machine for keyboard, focus, and ARIA state.

Import path:
- `@affino/datagrid-core/advanced`

## Core Contract

Source:
- `packages/datagrid-core/src/a11y/headlessA11yStateMachine.ts`

Public API:
- `createDataGridA11yStateMachine(...)`
- `DataGridA11yStateMachine`
- `DataGridA11ySnapshot`
- `DataGridA11yKeyboardCommand`
- `DataGridA11yGridAriaState`
- `DataGridA11yCellAriaState`

State machine guarantees:
- same command sequence => same snapshot (`deterministic`)
- roving tabindex contract (`active cell = 0`, others `-1`)
- stable `aria-activedescendant` derived from focused cell
- clamped focus after resize (`setDimensions`) with fail-safe blur for empty grids

Keyboard contract includes:
- arrows
- `Home` / `End` (with `ctrl/meta` row jump semantics)
- `PageUp` / `PageDown` (configurable page step)
- `Tab` / `Shift+Tab`
- `Escape` blur semantics

## Adapter Mapping Contract (Vue)

Source:
- `packages/datagrid-vue/src/adapters/a11yAttributesAdapter.ts`

Mapping helpers:
- `mapDataGridA11yGridAttributes(...)`
- `mapDataGridA11yCellAttributes(...)`

These convert headless A11y state to DOM-ready attributes:
- grid: `role`, `tabindex`, `aria-rowcount`, `aria-colcount`, `aria-activedescendant`
- cell: `id`, `role`, `tabindex`, `aria-rowindex`, `aria-colindex`, `aria-selected`

## Stage Selection Contract

The app stage mirrors the committed selection snapshot into rendered cell accessibility state:

- body cells expose `aria-selected="true"` for the active anchor cell and every rendered selected cell, including inactive additive ranges;
- unselected rendered cells expose `aria-selected="false"` so virtualized remounts return a deterministic selected state;
- row-selection checkbox cells keep `role="checkbox"` and `aria-checked` in sync with row-selection state;
- placeholder cells that cannot materialize into editable rows expose `aria-disabled="true"`;
- DOM focus restoration may lag virtualization, but selected/disabled state must be derived from the logical selection snapshot and row surface state, not from focus alone.

## Virtualized Stage DOM Mapping

The table stage exposes screen-reader-relevant grid metadata on the rendered virtualized viewport:

- the center body viewport exposes `role="grid"`, `aria-rowcount`, `aria-colcount`, and `aria-multiselectable="true"`;
- `aria-rowcount` reflects the logical row count from selection or viewport state, not only the number of mounted rows;
- `aria-colcount` reflects the rendered table-stage column model, including the internal row-selection column when present;
- rendered body rows expose `role="row"` and one-based `aria-rowindex` derived from the absolute row index;
- rendered center and pinned body cells expose one-based `aria-rowindex` and `aria-colindex`, with `role="gridcell"` unless an interactive cell role such as `checkbox` overrides it;
- rendered cells keep deterministic `aria-selected` state after virtualized unmount/remount;
- loading or placeholder cells that are not editable expose `aria-disabled="true"` while preserving their row/column index metadata.

## Contract Tests

- Core state machine:
  - `packages/datagrid-core/src/a11y/__tests__/headlessA11yStateMachine.contract.spec.ts`
- Vue adapter DOM mapping:
  - `packages/datagrid-vue/src/adapters/__tests__/a11yAttributesAdapter.contract.spec.ts`
- Stage selection accessibility:
  - `packages/datagrid-vue-app/src/__tests__/DataGrid.contract.spec.ts`
  - `packages/datagrid-vue-app/src/stage/__tests__/useDataGridStageCellState.spec.ts`
- Virtualized stage accessibility:
  - `packages/datagrid-vue-app/src/__tests__/DataGrid.contract.spec.ts`
  - `e2e/sandbox-interactions.spec.ts`
