# DataGrid Editing

This document defines the enterprise editing contract for the mounted DataGrid app.

## Ownership

- `datagrid-core` owns cell type parsing/formatting, draft validation helpers, row patch APIs, and datasource-backed optimistic commit/rollback.
- `datagrid-vue` owns the canonical app inline edit state machine in `useDataGridAppInlineEditing.ts`.
- `datagrid-vue-app` owns rendered editor DOM, focus surfaces, combobox overlays, pinned/body pane materialization, and stage event binding.
- Server-backed editing stays on `rows.applyEdits` and datasource `commitEdits`; server history remains the undo/redo owner for server datasource grids.

`datagrid-orchestration` editing helpers remain reusable/reference utilities. They are not the current mounted table-stage editing owner.

## State Machine

The app-stage editor has these supported states:

- `idle`: no active inline editor.
- `editing`: a row id, column key, draft value, editor mode, and optional select filter are active.
- `invalid`: the draft failed cell-type validation and remains in the editor.
- `committing`: a row patch was submitted and the editor is disabled while the commit settles.
- `rejected`: the commit failed or rolled back; successful edit history is not recorded.
- `cancelled`: the draft is discarded and focus returns to the grid cell.

The editor is identified by stable `rowId` and `columnKey`, not by DOM position. DOM focus can move between the grid viewport and an editor, but focus is not the source of truth for edit state.

## Commit And Cancel

- `Enter` commits and moves vertically.
- `Shift+Enter` commits and moves upward.
- `Tab` commits and moves to the next editable cell.
- `Shift+Tab` commits and moves to the previous editable cell.
- `Escape` cancels without applying the draft.
- Text/date blur commits unless the event is suppressed by cancel or a popup-owned interaction.

IME composition is protected. While composition is active, `Enter`, `Tab`, `Escape`, and printable keys must not trigger grid commit, cancel, navigation, or a new edit start.

## Validation

Inline editing validates drafts before mutating rows:

- number, currency, and percent drafts must parse to finite numeric values or an empty clear value
- date and datetime drafts must parse to valid dates or an empty clear value
- select drafts must match configured options when options are available
- formula cells in the table-stage DataGrid are basic text edits unless a host explicitly integrates a richer formula editor

Invalid drafts keep the editor open and expose invalid editor state. They must not mutate rows or record successful history.

## Clipboard Parity

Clipboard paste uses the same cell draft validation boundary as inline editing for supported cell types. Mixed editable/non-editable or valid/invalid ranges apply only valid editable cells and record history only for committed patches.

## Datasource Commits

Datasource-backed edits are optimistic below the app layer, but the app-stage contract is:

- pending edits expose a committing state and disable the active editor
- rejected/rolled-back commits do not record successful local history
- row state is reconciled through datasource invalidation, row snapshots, or rollback
- server-backed grids should use server stack history for undo/redo rather than client snapshot history

## Blur, Unmount, And Remount

If an active rendered editor leaves the mounted row/column window, the stage commits the edit with no selection/focus transfer. Popup focus owned by the combobox must not trigger unintended blur commit. Projection/cache replacement must explicitly commit, cancel, or reject the edit; it must not silently rebase a draft onto a different row.

## Accessibility

Rendered editors expose native input semantics. Invalid or rejected drafts expose `aria-invalid`; pending commits expose `aria-busy` where the native control supports it. Async select editors expose combobox/listbox roles and loading state.

## Unsupported Or Basic Scope

The table-stage DataGrid does not currently provide spreadsheet-class formula diagnostics, reference picking, formula autocomplete, collaborative merge UI, or durable offline edit replay. Host apps that need those capabilities must provide an explicit editor/datasource contract.
