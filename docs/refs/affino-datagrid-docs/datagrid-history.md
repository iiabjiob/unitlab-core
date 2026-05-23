# DataGrid History API

Updated: 2026-05-18

This document describes the public history contract exposed by `@affino/datagrid-vue-app`.

## Public prop

`DataGrid` accepts `history` as either:

- `true` / `false`
- `DataGridHistoryOptions`

```ts
type DataGridHistoryProp = boolean | {
  enabled?: boolean
  depth?: number
  shortcuts?: false | "grid" | "window"
  controls?: false | "toolbar" | "external-only"
  adapter?: DataGridTableStageHistoryAdapter
}
```

## Semantics

- `enabled: false` disables history entirely.
- `depth` limits recorded transactions/intents.
- `depth` does not count every raw cell keystroke as a separate undo step.
- `adapter` lets an application inject its own undo/redo source.
- When `adapter` is omitted, the app facade uses its built-in intent history.

## Enterprise History Modes

There are two supported history ownership modes today:

- Client snapshot history for local row models.
- Server stack history for server-backed datasource grids.

Client snapshot history is in-memory. It records before/after row snapshots for supported app intents and replays them through the app row model. It is suitable for client-only edit, paste, fill, range move, and placeholder-materialization flows where the grid owns the current row data.

Server stack history is persistent within the backend datasource scope. It records operation cell events for server edit/fill commits and replays them through scoped stack routes:

- `POST /api/history/undo`
- `POST /api/history/redo`
- `POST /api/history/status`

Server-backed grids should use server stack history as the normal undo/redo owner. They must not rely on client row snapshots for unloaded server rows, off-viewport materialization, or persisted undo/redo after reload.

## Ownership Boundary

Use one active history owner for a user action:

- Client-only rows: built-in intent history owns undo/redo unless `history.adapter` is supplied.
- Server-backed rows: datasource stack history owns undo/redo through the server adapter.
- External apps: `history.adapter` may replace built-in history, but it must expose the same controller behavior and own its own consistency guarantees.

The built-in app may still commit active editors, close transient menus, and refresh toolbar state before or after a history action. Those UI steps do not change the history owner for the data mutation.

## Stack Invariants

All built-in history paths follow these invariants:

- Undo and redo are explicit user actions, not automatic retries.
- Built-in transaction service actions are single-flight: `applyTransaction`, `commitBatch`, `undo`, and `redo` reject overlapping calls while an async history action is in progress.
- Built-in keyboard/control history runners ignore a second undo/redo trigger while the first trigger is pending.
- If undo fails after partially rolling back commands or transactions, the core transaction service re-applies the already-undone work and leaves the undo/redo stacks on the original side of the failed action.
- A new committed operation after undo invalidates the redo branch for the same history owner and scope.
- A grouped app intent, such as paste or cut-paste, is treated as one undo unit.
- Server undo/redo are state-changing mutations and must return revision/dataset-version state when available.
- Server stack undo/redo is scoped by workspace, table, and user and/or session.
- Legacy operation-id replay routes are diagnostics/manual replay paths, not normal UX stack navigation.

Core transaction batches provide lower-level grouping for package internals. App-level intent grouping is the public behavior exposed by `@affino/datagrid-vue-app`.

## Persistence And Recovery

Client snapshot history is not durable:

- It is lost on page reload.
- It is not serialized as an operation log.
- It does not provide cross-tab, offline, or device-restart recovery.
- It can only restore rows that were captured in the current in-memory session.

Server stack history is durable within the backend table/history scope:

- It survives frontend reload when the backend preserves the operation stack.
- It is ordered with datasource `revision` / `datasetVersion`.
- It emits invalidation or row snapshots so the client can refresh affected data.
- It currently covers server edit/fill cell events, not every possible DataGrid operation.

Reconnect is read/live recovery unless a host app implements an explicit durable mutation contract. Failed edits, fill commits, undo, and redo are not automatically retried by the built-in client history.

## Snapshot Semantics

Client snapshot history restores captured row state, not normalized per-cell operation payloads. For row-scoped edits this is practical, but it has limits:

- A row snapshot can overwrite unrelated fields changed after the snapshot was captured.
- Full-model fallback snapshots are guarded by built-in row, cell, and byte-estimate budgets.
- The built-in client budget defaults are 10,000 rows, 250,000 estimated cells, and 16 MiB estimated JSON payload per captured snapshot.
- If a before or after snapshot exceeds budget, that intent is not recorded as a history entry.
- Structural operations that cannot be scoped by stable row ids may be skipped by history when they exceed budget.

Enterprise integrations that need persisted client operations, reload recovery, conflict replay, or operation inspection should treat versioned operation payloads as planned work rather than current behavior.

## Operation Metadata

Built-in client history now records additive versioned operation metadata beside snapshot payloads. Snapshots remain the replay source for client undo/redo; operation metadata is for inspection, diagnostics, and future migration paths.

Current operation payload shape:

- `version: 1`
- `kind`: `edit`, `paste`, `cut-paste`, `fill`, `range-move`, `row-insert`, `row-delete`, `placeholder-materialization`, or `snapshot-fallback`
- `intent`: normalized app intent string
- `scope.snapshotKind`: `partial`, `full`, or `snapshot-fallback`
- `scope.rowIds`
- `scope.rowCount`
- optional `scope.affectedRange`
- optional `metadata`

If an app supplies an explicit version-1 operation payload, it is normalized and recorded. Otherwise the built-in app history derives operation metadata from the intent descriptor and captured before/after snapshots. Unsupported or broad structural operations continue to replay through snapshots and are marked as `snapshot-fallback`.

## Restoration Semantics

History restores data first. Built-in client app history can also carry an optional restoration payload captured with each before/after row snapshot.

Currently supported restoration payload fields:

- active cell
- selection snapshot/ranges
- scroll anchor
- focus target
- edit target metadata

The Vue app stage wires active cell, selection snapshot, scroll anchor, and focus target for built-in client history. After undo/redo, the stage restores the captured selection snapshot, then requests the captured focus target with `preventScroll` behavior through the existing active-cell viewport path.

Inline editor content and formula-edit state are metadata only in the current payload; they are not reopened as editors after undo/redo. Server-backed grids still refresh visible rows after invalidation and do not use client restoration payloads as a persisted server history contract.

## Collaboration And Conflict Semantics

Current collaboration behavior is explicit but limited:

- Server-backed mutations use revision and projection tokens to reject stale edits/fill commits where supported.
- Server stack history is scoped by workspace, table, and user/session.
- Server stack undo/redo for cell events checks the current cell value before replay. If another user/session has changed the same cell since the original operation, the backend rejects that cell with `history-conflict` and does not overwrite the remote value.
- There is no collaborative merge model for client snapshot history.
- Structural row operations, column operations, selection state, formula-edit state, grouping/tree/pivot state, and workbook-level operations are not part of the current server stack history contract unless a host backend adds an explicit capability.

If a host app supports multi-user editing, it should prefer server-backed history with explicit revision, dataset-version, operation-id, and scope handling. Client snapshot history should be treated as single-session local undo/redo.

## Shortcuts

- `false`: no built-in keyboard shortcuts
- `"grid"`: shortcuts are active while the grid owns keyboard focus
- `"window"`: shortcuts are bound at `window` scope

## Controls

- `false`: no built-in visual controls
- `"toolbar"`: render built-in `Undo` / `Redo` toolbar buttons
- `"external-only"`: no built-in controls, but keep the controller exposed for external UI

## Exposed controller

The component ref exposes a stable history controller regardless of whether history comes from the built-in implementation or an injected adapter:

```ts
interface DataGridHistoryController {
  canUndo(): boolean
  canRedo(): boolean
  runHistoryAction(direction: "undo" | "redo"): Promise<string | null>
}
```

Access it through:

- `gridRef.value?.history`
- `gridRef.value?.getHistory()`

## Example

```vue
<script setup lang="ts">
import { ref } from "vue"
import { DataGrid } from "@affino/datagrid-vue-app"

const gridRef = ref<{
  getHistory?: () => {
    runHistoryAction?: (direction: "undo" | "redo") => Promise<string | null>
  }
} | null>(null)

async function redo() {
  await gridRef.value?.getHistory()?.runHistoryAction("redo")
}
</script>

<template>
  <button type="button" @click="redo">
    Redo
  </button>

  <DataGrid
    ref="gridRef"
    :rows="rows"
    :columns="columns"
    :history="{ depth: 100, shortcuts: 'grid', controls: 'external-only' }"
  />
</template>
```
