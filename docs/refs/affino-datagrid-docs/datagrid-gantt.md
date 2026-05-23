# DataGrid Gantt

Affino DataGrid Gantt is a renderer over the existing grid row pipeline.

It does not own task data, does not introduce a parallel task store, and does not bypass virtualization.

## Architecture

Source of truth:

- data store
- formula engine
- projection pipeline
- visible rows
- table / gantt renderers

Gantt consumes the same row nodes already produced by the grid runtime.

It reads scheduling fields directly from row cells:

- `start`
- `end`
- `baselineStart`
- `baselineEnd`
- `progress`
- `dependencies`
- `critical`

View relationship:

- grid owns data
- gantt derives render instructions
- edits flow back through grid row mutations

## Rendering Model

Left pane:

- existing grid columns

Right pane:

- timeline header
- canvas body
- dependency routing

Performance rules:

- bars are rendered on canvas
- only `displayRows` / `visibleRows` are rendered
- vertical scroll is shared with the grid
- horizontal scroll belongs to the timeline viewport

## Summary Tasks

Summary tasks are derived, not stored.

Current behavior:

- if a visible row is a grid `group` row
- and it has visible descendant tasks with valid `start/end`
- gantt derives a summary bar from:
  - `start = min(descendant.start)`
  - `end = max(descendant.end)`

Important constraint:

- summary derivation currently works from visible rows only
- this keeps gantt aligned with collapsed / expanded projection output
- collapsed groups therefore do not require a parallel gantt hierarchy model

If a collapsed group must still show a summary range when descendants are not visible, that range should come from the grid row itself or from a future derived pipeline field.

## Baseline Bars

Baseline ranges are read directly from row cells.

Typical fields:

- `baselineStart`
- `baselineEnd`

Behavior:

- baseline bars are rendered behind the active task bar
- baseline variance markers show start / finish slippage against the current task range
- gantt does not store baseline data separately
- baseline is skipped when the row has no valid baseline range
- summary bars do not derive a separate baseline range

## Working Calendar

Gantt supports a headless working calendar.

Config shape:

```ts
type DataGridWorkingCalendar = {
  workingWeekdays?: readonly number[]
  holidays?: readonly (Date | string | number)[]
}
```

Behavior:

- timeline paints non-working spans
- default calendar is Monday to Friday working days
- holidays are treated as non-working days
- day-level drag / resize snaps through working days

Week and month zoom still snap by week boundaries.

## Dependencies

Dependencies are read from row cells through `dependencyKey`.

Matching behavior:

- first by `idKey`
- fallback by `rowId`

Supported dependency types:

- `FS` finish-to-start
- `SS` start-to-start
- `FF` finish-to-finish
- `SF` start-to-finish

Accepted predecessor syntax:

- `task-a`
- `task-a:SS`
- `task-a->FF`
- `12FS`

Dependency rendering is currently visible-to-visible only.

That means:

- if both bars are visible, the link is drawn
- if a source or target row is outside the current visible gantt rows, the link is omitted

This is intentional for the current renderer contract and keeps dependency drawing aligned with row virtualization.

## Critical Path

Gantt supports two critical highlighting sources:

- explicit row flag through `criticalKey`
- computed critical path through `computedCriticalPath: true`

Current computed behavior:

- tasks are read from the row model
- dependencies are resolved as finish-to-start edges
- the engine performs a forward / backward pass on the dependency graph
- tasks with zero slack are marked critical

Current limitation:

- computed critical path is based on task date ranges and dependencies
- computed critical path currently evaluates `FS` edges only
- it is not yet resource-aware
- it is not yet calendar-aware beyond the task dates already stored in rows
- cyclic dependency graphs currently disable computed critical highlighting

## Interaction Rules

Dragging a bar:

- computes a new range in gantt runtime
- commits through `rows.applyEdits(...)`
- updates the same `start/end` cells used by the table

Selection:

- clicking a dependency highlights both linked bars and the link
- clicking empty canvas clears selection
- `Escape` clears selection

Summary bars are derived and are not draggable.

## Public Surface

Primary Vue prop:

```ts
<DataGrid :gantt="{
  idKey: 'id',
  labelKey: 'task',
  startKey: 'start',
  endKey: 'end',
  baselineStartKey: 'baselineStart',
  baselineEndKey: 'baselineEnd',
  progressKey: 'progress',
  dependencyKey: 'dependencies',
  criticalKey: 'critical',
  computedCriticalPath: true,
  zoomLevel: 'week',
  workingCalendar: {
    workingWeekdays: [1, 2, 3, 4, 5],
    holidays: ['2026-12-25'],
  },
}" />
```

View switching:

- `table`
- `gantt`

## Current Enterprise Features

Implemented:

- canvas bars
- baseline bars
- dependency routing
- dependency types (`FS`, `SS`, `FF`, `SF`)
- dependency selection
- progress overlay
- milestones
- resizable split view
- today marker
- zoom
- working calendar
- derived summary tasks for visible group rows
- computed critical path from row dependencies

Planned next:

- summary ranges sourced from pipeline-level derived fields when needed
