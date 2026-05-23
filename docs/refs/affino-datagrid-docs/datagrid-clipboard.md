# DataGrid Clipboard

This document defines the enterprise clipboard contract for the mounted DataGrid app.

## Ownership

- `datagrid-orchestration` owns the reusable clipboard bridge: copy payload creation, browser clipboard read/write, in-memory fallback, copied-range flash state, and basic clipboard matrix parsing.
- `datagrid-vue` owns the canonical app clipboard state machine in `useDataGridAppClipboard.ts`: copy, cut, paste, paste-special values, pending ranges, selection normalization, validation, history capture, and local mutation safety checks.
- `datagrid-vue-app` owns rendered keyboard/context-menu bindings, pending clipboard outlines, row-index clipboard actions, placeholder row materialization, pinned/body pane rendering, and status surfaces.
- `datagrid-core` owns virtual-selection operation decisions and cell draft validation helpers consumed by the app clipboard path.
- Server-backed clipboard work must stay aligned with datasource revisions, projection identity, invalidation, and server history. Unloaded virtual clipboard operations are currently blocked unless a future server delegate is approved and configured.

`useDataGridClipboardMutations.ts` remains a reusable/reference helper. It is not the mounted table-stage owner.

## Supported Operations

- `Ctrl/Cmd+C` and the cell context menu copy the resolved active selection range.
- `Ctrl/Cmd+X` and the cell context menu cut by staging a copy operation and later clearing editable source cells during paste.
- `Ctrl/Cmd+V` and the cell context menu paste a parsed clipboard matrix at the active cell or selected target range.
- `Paste special -> Values only` uses the same app clipboard pipeline and can reuse an internal copied range when available.
- Scalar paste into multiple committed ranges is supported; the scalar value is applied to each normalized target range.
- Row-index copy/cut/paste actions are app-renderer row operations. They keep a trusted same-session row payload for DataGrid paste and write external TSV/HTML fallbacks for spreadsheet paste.

Clipboard behavior is active only in base table mode.

## Copy Contract

Copy resolves one source range through the current selection/current-cell helpers. Pending clipboard visuals can retain each committed selection range, but the current copy payload is active-range based.

Before copying or cutting, the app blocks local materialized operations when the source range includes:

- stale virtual selection metadata
- unloaded rows
- placeholder rows that cannot be read safely
- grouped/tree projection rows

Current cell copy writes plain text TSV to the system clipboard when permitted and always stores the copied payload in the in-memory fallback. Row-index copy writes `application/x-affino-datagrid-rows+json` when the browser supports custom clipboard MIME types, keeps a same-session tokenized row payload in memory, and writes `text/plain` TSV plus `text/html` table fallbacks over visible copyable data columns. The row index/system columns are excluded, and raw row JSON must not be written as `text/plain`. The TSV writer quotes fields that contain tabs, newlines, or quotes and escapes embedded quotes by doubling them.

## Paste Contract

Paste reads from the system clipboard when permitted and falls back to the last in-memory payload. Row-index paste prefers a trusted compatible row payload; `cut` becomes a move only when the same-session token, source instance, and schema match. Custom or legacy row payloads without a matching session token are treated as copy/import, and spreadsheet text falls back to matrix paste semantics. Empty payloads do not paste.

The app parses the payload into a matrix, resolves target ranges, blocks unsafe virtual/group/unloaded targets, materializes editable placeholder rows when the host provides `ensureEditableRowAtIndex`, and applies row patches through `rows.applyEdits` or a host-provided `applyClipboardEdits`.

Matrix semantics:

- Multi-cell matrices paste from the active cell unless the selected range is used as an explicit target.
- Scalar matrices can paste into every committed selection range.
- Matrix values repeat across larger target ranges.
- Ragged rows currently treat missing cells as empty strings.
- A single terminal row separator is treated as a payload terminator; additional blank rows are preserved.

The canonical local app path reports structured paste counts for target, applied, blocked, skipped, and invalid cells. The public `applyClipboardEdits` return value remains the existing updated-row count for API stability.

## Validation

Clipboard paste uses the shared cell draft validation boundary for supported typed cells before row mutation. Invalid typed drafts are skipped and do not mutate their target cell. Supported validation follows the same core helpers used by inline editing for number, currency, percent, date, datetime, select, empty clear values, and basic formula text behavior.

Mixed editable/non-editable or valid/invalid ranges apply only valid editable cells. History records only committed patches.

## Cut-Paste And History

Paste and cut-paste participate in app intent history:

- normal paste records one edit transaction when row patches are committed
- multi-range scalar paste records one merged transaction where possible
- local cut-paste applies source clear and target write as one row-model commit, then records one transaction

Custom/server cut-paste handlers must provide their own atomicity or reject before mutating. Local row-model cut-paste failures keep source cells intact and surface rejected paste state.

## Async Paste State

The app clipboard path exposes paste state as `idle`, `pending`, or `rejected`. While paste is pending, duplicate paste commands are blocked. If an async local, custom, or server paste handler rejects, the command returns `false`, a rejected state is retained for recovery UI, and a user-facing failure message is reported.

## Server And Virtual Ranges

Loaded materialized rows can use the local clipboard path. Stale virtual ranges remain blocked. Unloaded virtual copy, cut, and paste ranges can use explicit opt-in server clipboard delegates; without those delegates, they remain blocked with a user-facing status. Grouped virtual operation semantics stay backend-defined and should remain blocked unless a host documents group behavior.

Future server clipboard operations should reuse the operation model in `docs/server-datasource/selection-operations.md` and the planned protocol shape in `docs/server-datasource/protocol.md`: operation id, base revision, projection identity, normalized ranges, stable column keys, payload format, invalidation, warnings, partial results, and server history state.

## Browser Clipboard Fallback

Browser clipboard APIs can fail because of permissions, secure-context rules, missing user gestures, or browser policy. Current behavior is safe-biased:

- copy write failures still keep an in-memory payload for same-session grid paste
- paste read failures fall back to the in-memory payload
- copy and paste report when the system clipboard is unavailable and an in-memory fallback is used

Structured permission diagnostics beyond the user-facing status message remain planned work.

## Accessibility And Mobile

The current app exposes visual pending clipboard outlines and routes grid status messages through a polite `role="status"` live region. Clipboard fallback failures are covered at the app contract level; broader permission-denied browser cases and partial paste announcements remain planned validation work. Escape clears pending cell and row-index clipboard intent from either the body grid or a focused row-index cell.

Touch selection remains scroll-first. Coarse-pointer browser coverage verifies long-press cell selection followed by keyboard copy/paste while body touch pan remains native-first. Real-device mobile clipboard behavior should use explicit affordances or OS-native behavior rather than body-cell drag gestures.

Browser validation covers copied and cut clipboard outlines across vertical remounts, horizontal remounts, and right-pinned pane remounts.

## Performance Gates

`bench:datagrid:enterprise:clipboard:assert` gates the current materialized copy/paste path for copy creation, TSV parser cost, paste payload creation, paste patch application, and total paste latency. `bench:datagrid:enterprise:clipboard:browser:assert` gates Chromium clipboard write/read/round-trip latency with granted clipboard permissions. Real-device mobile clipboard workflows remain planned gates.

## Unsupported Or Planned Scope

The mounted table-stage DataGrid does not currently provide:

- CSV parser/writer compatibility
- full multi-MIME browser compatibility across all engines
- rich HTML clipboard modes beyond the row-index table fallback
- formula/format paste modes beyond values
- built-in HTTP routes for server-delegated copy/export/cut/clear/paste over unloaded virtual ranges
- rendered async paste retry/cancel UI
- per-cell rejection UI and durable paste telemetry
- real-device mobile clipboard UX beyond existing keyboard/context-menu paths
