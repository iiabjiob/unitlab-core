# DataGrid Chrome Engine

`@affino/datagrid-chrome` is the headless geometry engine for table chrome.

It does not own data, DOM, or canvas lifecycle.
It only projects pane-local line positions from existing table layout state.

## Purpose

Use the package when a renderer needs shared grid chrome without duplicating pane math:

- row divider positions
- column divider positions
- row background bands for striping / group / tree / pivot states
- pinned left / center / right pane geometry
- center viewport scroll projection

This keeps framework adapters thin:

- Vue: canvas overlay renderer
- Laravel / Livewire: canvas or SVG renderer
- future adapters: same geometry model, different host wiring

## Ownership

The package is intentionally headless.

It does not do:

- DOM reads
- `ResizeObserver`
- scroll listeners
- device pixel ratio handling
- canvas drawing

Those stay in the adapter layer.

## Core API

```ts
import { buildDataGridChromeRenderModel } from "@affino/datagrid-chrome"

const chrome = buildDataGridChromeRenderModel({
  rowMetrics,
  scrollTop,
  leftPaneWidth,
  centerPaneWidth,
  rightPaneWidth,
  viewportHeight,
  leftColumnWidths,
  centerColumnWidths,
  rightColumnWidths,
  centerScrollLeft,
})
```

The result is split by pane:

- `chrome.left`
- `chrome.center`
- `chrome.right`

Each pane exposes:

- `width`
- `height`
- `bands`
- `horizontalLines`
- `verticalLines`

Line positions are returned in CSS pixel space relative to the pane viewport.
Adapters are responsible for final pixel snapping during draw.

Band geometry is also returned in pane-relative CSS pixels.
The engine does not choose colors; adapters map `band.kind` to theme tokens.

## Optional Clipping Hints

For adapters that already know visible index windows, the engine also accepts optional range hints:

- `visibleRowRange`
- `leftVisibleColumnRange`
- `centerVisibleColumnRange`
- `rightVisibleColumnRange`

These are performance hints only.
They let the engine skip geometry outside the requested row or column indexes without introducing a second source of truth.
The indexes are relative to the arrays passed into the engine, not to the full grid dataset.

If omitted, the engine projects the full input arrays.

## Architecture

```text
DataGrid row/column layout
  -> adapter measurements
  -> @affino/datagrid-chrome
  -> pane line model
  -> framework renderer
```

This follows the same principle as Gantt:

- Grid owns layout inputs
- chrome engine derives render geometry
- framework adapter draws it

## Notes

- `rowMetrics` should come from the same layout truth the table uses.
- `centerColumnWidths` may include left/right spacers when the center viewport renders virtualized columns with outer padding.
- The engine is render-model only. It should not grow selection, editor, or interaction state.
