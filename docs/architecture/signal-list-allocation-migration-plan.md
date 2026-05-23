# Signal List and Allocation Migration Plan

Status: Slice 7 complete, Slice 8 next
Last reviewed: 2026-05-23

## Scope

This note consolidates the signal-list, allocation, live-test, DataGrid, and allocation-UX audits into a staged migration plan.

The goal is a high-performance FAT signal-list editor that stays responsive at about 20,000 rows while backend services remain the source of truth for allocation, validation, test execution, and evidence.

## Current Architecture

### Data Flow

```text
Import workbook
  -> backend parses rows
  -> signals table + signal_sheets singleton metadata
  -> signal_allocations table stores active signal/channel links
  -> REST/NDJSON returns flat SignalAllocationRow projection
  -> frontend stores allocationRows[]
  -> SignalsPage maps allocationRows[] to gridRows[]
  -> Affino DataGrid receives full rows prop
```

Current projection rows combine:

- signal identity and source metadata;
- allocation channel id/type/index/label;
- device/unit id and online state;
- tested timestamp.

### Allocation

Implemented:

- `signal_allocations` is an explicit backend table.
- Backend has uniqueness for one allocation per signal and one signal per channel within a workspace.
- Backend validates unknown signals, unknown channels, occupied channels, and signal/channel type compatibility.
- Frontend uses a signal-row side panel picker, not a heavy select in every row.
- assign/reassign/unassign/swap are first-class backend actions returning changed row patches.
- async allocation jobs return changed rows for targeted frontend patching.
- auto allocation and bulk unassign have a preview step before apply.

Gaps:

- conflict, invalid type, missing, stale, and offline health are visible in the grid and quick filters; dedicated resolution workflows are still pending.
- durable allocation event history is still pending.

### Live Test Updates

Implemented:

- auto-test jobs run in a backend worker.
- job progress is published over WebSocket.
- tested timestamps can be sent as per-signal patches inside job event result payloads.

Gaps:

- runtime test fields are mixed into signal-list row data.
- frontend updates still cause broad computed-row churn.
- there is no dedicated row/cell patch stream contract.
- live fields can be sorted/filtered without a clear projection policy during active test runs.

### Frontend Grid

Implemented:

- Affino DataGrid uses stable row ids through `resolveRowId`.
- the app uses a client row model and row/column virtualization.
- Affino core supports row patching, batch boundaries, and cell refresh APIs.

Gaps:

- app code passes a newly computed full `gridRows` array.
- app does not use `api.rows.patch`, `api.rows.batch`, or `api.view.refreshCellsByRowKeys` as the normal live update path.
- Vue reactivity owns the full allocation row array, which is risky for hot paths.

## Target Architecture

```text
Backend domain state
  signals / revisions / devices / channels / allocations / test runs
        |
        | initial load
        v
Flat SignalAllocationProjection rows
        |
        v
Frontend grid client row model
        |
        | allocation/test/device updates
        v
Patch queue -> DataGrid row/cell patches
```

Ownership:

- Backend owns allocation truth, validation, transactions, uniqueness, conflict detection, and test execution.
- Frontend owns operator interaction state, visible filters, side-panel picker state, and DataGrid patch application.
- DataGrid owns viewport, virtualization, selection, editing, and row/cell rendering.
- WebSocket/SSE owns live patch delivery, not full-table refresh.

Core rules:

- initial signal list load can load all rows for up to about 20,000 rows;
- allocation and test updates should return or emit changed rows/cells only;
- full reload is a recovery path for reconnect, patch gaps, or version mismatch;
- test evidence should not depend on frontend transient state.

## Main Gaps To Close

| Gap | Current state | Target state |
| --- | --- | --- |
| Frontend row updates | full array recompute | row/cell patch queue |
| Allocation actions | generic bulk update + auto jobs | explicit assign/reassign/unassign/swap/bulk actions |
| Async job result | changed ids, then reload | changed row patches |
| Auto allocation | preview before apply | richer compatibility warnings and apply parity tests |
| Runtime test state | mixed into row data | patch stream, optionally separate runtime store |
| Allocation health | projection fields + grid badges/filters | resolution workflows and event history |
| Conflict UX | mostly hidden | visible conflict state and resolution actions |
| Revision safety | workspace-level active signals | explicit signal-list revisions used by tests/reports |

## Proposed Data Model Changes

### Keep

- `signals`
- `signal_allocations`
- `devices`
- `channels`
- current uniqueness constraints on active allocation ownership.

### Add Or Evolve

#### `signal_list_revisions`

Fields:

- `id`
- `workspace_id`
- `source_hash`
- `source_filename`
- `import_meta`
- `rows_count`
- `created_at`
- `activated_at`
- `is_active`

Purpose:

- make imports and retests traceable;
- bind test runs and reports to an immutable signal-list revision.

#### `signal_items`

Fields:

- `id`
- `revision_id`
- `key`
- `name`
- `io_direction`
- `category`
- `metadata`
- `row_order`

Purpose:

- make a signal item revision-scoped instead of only workspace/key scoped.

#### `allocations`

Either evolve `signal_allocations` or add a versioned replacement.

Required fields:

- `id`
- `workspace_id`
- `revision_id`
- `signal_item_id`
- `channel_id`
- `status`: active, superseded, removed
- `allocation_meta`
- `created_by`
- `created_at`
- `updated_at`

Constraints:

- one active allocation per signal item;
- one active allocation per channel per workspace/revision;
- historical rows remain available for test-run/report evidence.

#### `allocation_events`

Append-only history for:

- assign;
- unassign;
- reassign;
- swap;
- auto-allocate;
- bulk apply;
- conflict resolution.

#### `test_run_steps`

Each executable step should capture:

- signal item id;
- allocation id;
- channel id;
- expected output/input behavior;
- command id;
- command payload;
- acknowledgement state;
- observed state;
- timestamp;
- status;
- operator override reason when applicable.

## Proposed API Changes

Do not break existing routes immediately. Add versioned or additive contracts first.

### Projection

`GET /api/v1/workspaces/{workspace_id}/signal-allocations/projection`

Returns:

```ts
type SignalAllocationProjectionRow = {
  rowId: string
  workspaceId: number
  revisionId: number | null
  signalItemId: number
  signalKey: string
  signalName: string
  signalDirection: "DI" | "DO" | "AI" | "AO"
  signalMetadata: Record<string, unknown>
  allocationId: number | null
  allocationStatus: "unassigned" | "assigned" | "conflict" | "invalid" | "missing"
  allocationHealth: {
    conflict: boolean
    invalidType: boolean
    missingDevice: boolean
    missingChannel: boolean
    offlineDevice: boolean
    staleDevice: boolean
  }
  channelId: number | null
  channelType: string | null
  channelIndex: number | null
  channelLabel: string | null
  deviceId: number | null
  unitId: string | null
  unitOnline: boolean | null
  unitLastSeenAt: string | null
  testedAt: string | null
}
```

### Mutations

Add explicit actions:

- `POST /signal-allocations/actions/assign`
- `POST /signal-allocations/actions/unassign`
- `POST /signal-allocations/actions/reassign`
- `POST /signal-allocations/actions/swap`
- `POST /signal-allocations/actions/bulk/preview`
- `POST /signal-allocations/actions/bulk/apply`
- `POST /signal-allocations/auto/preview`
- `POST /signal-allocations/auto/apply`

Response shape:

```ts
type AllocationMutationResponse = {
  workspaceId: number
  revisionId: number | null
  eventId: string
  changedRows: SignalAllocationProjectionRow[]
  conflicts: AllocationConflict[]
  rejected: AllocationRejectedItem[]
}
```

Rules:

- all mutation endpoints run in backend transactions;
- no silent overwrite;
- swap is atomic;
- conflicts return structured data;
- frontend does not infer allocation truth.

### Live Patches

Add WebSocket/SSE events:

- `signal_allocation_rows_patched`
- `signal_test_runtime_patched`
- `device_channel_state_patched`

Patch shape:

```ts
type RowPatchEvent = {
  type: string
  workspaceId: number
  revisionId: number | null
  sequence: number
  patches: Array<{
    rowId: string
    signalItemId: number
    changes: Record<string, unknown>
    columns?: string[]
  }>
}
```

Frontend must reload projection when:

- sequence gap is detected;
- revision id changes;
- patch references an unknown row;
- backend sends `requiresFullReload: true`.

## Proposed Frontend Store Changes

### Current

```text
allocationRows: ref<SignalAllocationRow[]>
displayAllocationRows: computed(map all rows)
gridRows: computed(map all rows)
DataGrid rows prop gets full gridRows array
```

### Target

```text
signalProjectionStore
  rowsById: Map<RowId, ProjectionRow>
  rowOrder: RowId[]
  projectionVersion

signalRuntimeStore
  runtimeByRowId: Map<RowId, RuntimeState>
  pendingRuntimePatches

signalGridPatchQueue
  enqueueRowPatch(rowId, changes)
  enqueueCellRefresh(rowIds, columns)
  flush in requestAnimationFrame
```

Grid strategy:

- initial load calls DataGrid `setRows` once;
- allocation mutations call `api.rows.patch`;
- runtime test fields call `api.rows.patch` only when sort/filter participation is required;
- volatile display-only fields can live in a runtime store and use `refreshCells`;
- full reload is explicit and rare.

## Proposed DataGrid API Changes

Existing Affino APIs are sufficient for the first implementation:

- `api.rows.patch(...)`
- `api.rows.batch(...)`
- `api.view.refreshCellsByRowKeys(...)`

Useful additions:

- `api.rows.patchRow(rowId, changes, options?)`
- `api.cells.patchCell(rowId, columnKey, value, options?)`
- `api.cells.refresh({ rowIds, columns, immediate?, reason? })`
- diagnostics reporting patch scope and projection recompute behavior.

## Proposed UX Changes

Keep:

- signal-item centric grid;
- row-level side-panel channel picker;
- no heavy select inside every row.

Add:

- quick filters: unassigned, assigned, conflicts, invalid type, offline/missing device, signal type, cabinet/source column, device/unit;
- channel picker states: free, current, occupied, incompatible, offline, missing;
- occupied channel owner display;
- explicit swap/move actions;
- auto-allocation preview;
- bulk allocation preview and apply;
- structured rejected/conflict result display after bulk operations.

## Slice Plan

### Slice 0 - Planning Document

Status: done in this document.

Goal:

- preserve audit findings and target architecture.

Validation:

- documentation review only.

Rollback:

- revert doc/config changes.

### Slice 1 - Projection Contract Baseline

Status: done.

Goal:

- add row identity and health/status fields to the flat projection without changing UI behavior.

Backend:

- extend projection schema with `row_id`, `allocation_id`, `allocation_status`, `allocation_health`.
- keep existing fields for compatibility.

Frontend:

- accept optional new fields in types.
- keep existing rendering.

Tests:

- backend allocation helper coverage for assigned/unassigned/offline/missing/incompatible states.
- frontend type check.

Rollback:

- frontend ignores optional fields;
- backend can stop sending new fields.

### Slice 2 - Frontend Grid Patch Queue

Status: done.

Goal:

- introduce a patch queue that can patch DataGrid rows/cells while preserving current full reload behavior.

Frontend:

- add `useSignalGridPatchQueue`.
- wire it behind a feature flag or internal path.
- keep `gridRows` prop path as fallback.

Tests:

- unit tests for coalescing patches by row id;
- frontend type check.
- browser-level selection and scroll preservation still needs manual verification under live updates.

Rollback:

- disable patch queue and keep full-row prop flow.

### Slice 3 - Allocation Job Changed Rows

Status: done.

Goal:

- stop full allocation reload after successful allocation jobs.

Backend:

- include `changed_rows` in terminal allocation job result.
- keep `changed_signal_ids` for compatibility.

Frontend:

- apply `changed_rows` to store and grid patch queue.
- fallback to full reload when changed rows are missing.

Tests:

- allocation worker serialization test for terminal `changed_rows` payload;
- frontend type check;
- existing full reload fallback remains when a job result has no changed row payload.

Rollback:

- frontend fallback reload remains.

### Slice 4 - Explicit Single-Row Allocation Actions

Status: done.

Goal:

- replace generic single-row bulk update UX path with explicit backend actions.

Backend:

- add assign, unassign, reassign endpoints.
- return changed rows and structured conflicts.

Frontend:

- channel picker calls explicit actions.
- show conflict/rejection messages.

Tests:

- write-service tests for assign, unassign, and reassign action semantics;
- frontend type check;
- grid patch queue regression test.
- browser-level picker conflict verification still needs manual validation.

Rollback:

- picker falls back to existing `setAllocation`.

### Slice 5 - Atomic Swap

Status: done for active allocation swap path; allocation event records remain pending until the `allocation_events` slice.

Goal:

- support safe swap between two allocated signals.

Backend:

- transactionally swap channel allocations.
- defer durable allocation event records to the explicit `allocation_events` migration.

Frontend:

- show occupied channel owner and `Swap` action in picker.

Tests:

- swap success;
- swap with missing signal/channel;
- swap with incompatible type;
- uniqueness preserved after failure.
- current coverage verifies write-service delegation and existing allocation action behavior; repository-level DB constraint tests are still needed.

Rollback:

- hide swap action.

### Slice 6 - Allocation Health and Filters

Status: done.

Goal:

- make conflict/invalid/offline/missing states visible and filterable.

Backend:

- compute health fields in projection.

Frontend:

- add status badges and quick filters.
- implemented signal-grid `Allocation` and `Health` columns backed by flat projection fields.
- implemented quick filters for all, unassigned, assigned, issues, conflicts, invalid, and offline/missing rows.
- quick filters are computed over the loaded client-side projection; channel occupancy lookup still uses the full projection so filtered-out owners are not treated as free.

Tests:

- projection fixtures;
- DataGrid filter tests;
- visual/manual verification for badge states.
- current frontend coverage includes pure allocation health/filter helper tests and existing patch queue tests.

Rollback:

- hide quick filters and badges; projection fields remain harmless.

### Slice 7 - Auto/Bulk Allocation Preview

Status: done.

Goal:

- prevent silent large changes.

Backend:

- add dry-run preview endpoints returning proposed bindings, skipped rows, conflicts, and warnings.
- implemented `POST /workspaces/{workspace_id}/signal-allocations/auto/preview`.
- implemented `POST /workspaces/{workspace_id}/signal-allocations/preview`.
- preview paths are read-only and backend validation still runs again on apply.

Frontend:

- add preview panel before apply.
- selected auto-assign and bulk unassign now open an allocation preview modal before enqueueing jobs.
- modal blocks apply when preview contains conflicts or rejected rows.

Tests:

- preview/apply parity;
- no overwrite without explicit option;
- large selection performance.
- current backend coverage verifies read-only service delegation, auto preview assignment, and bulk conflict preview.
- current frontend validation covers type contracts; browser visual verification remains manual.

Rollback:

- keep old auto allocation button behind fallback.

### Slice 8 - Runtime Test Patch Stream

Goal:

- avoid full row churn during test runs.

Backend:

- emit `signal_test_runtime_patched` events with row ids and changed fields.

Frontend:

- batch patches in rAF.
- patch only status/value/timestamp cells.

Tests:

- WebSocket patch ordering;
- reconnect gap recovery;
- 1,000 patch burst while scrolling.

Rollback:

- continue using existing job event result patches and full reload fallback.

### Slice 9 - Separate Runtime State

Goal:

- isolate volatile test state from static signal/allocation rows.

Frontend:

- keep static grid row data stable.
- store runtime test values separately when they do not need sort/filter participation.

Tests:

- visible cell refresh test;
- non-visible row update test;
- sort/filter policy tests.

Rollback:

- fold runtime fields back into row patches.

### Slice 10 - Signal List Revisions

Goal:

- make tests and reports revision-safe.

Backend:

- add revisions and revision-scoped signal items.
- bind allocation and test run evidence to revision ids.

Frontend:

- show active revision and stale allocation indicators.

Tests:

- import creates revision;
- report remains tied to original revision;
- retest reuse compatibility.

Rollback:

- dual-read legacy workspace-scoped signals until migration is stable.

### Slice 11 - Performance Gate

Goal:

- prove 20,000-row workflows.

Benchmarks:

- initial 20k projection load;
- patch 1 visible row;
- patch 1 non-visible row;
- apply 5k allocation patches;
- apply 5k test runtime patches;
- scroll during patch burst;
- sort/filter with live patch policy.

Acceptance targets:

- no full reload in normal allocation/test patch paths;
- selection/focus/scroll preserved;
- no blank viewport;
- no unbounded patch queue growth.

## Risks

| Risk | Mitigation |
| --- | --- |
| Patch stream drift | sequence numbers and full reload fallback |
| Live sort/filter row movement | freeze projection during active test run or require explicit reapply |
| Revision migration breaks retest reuse | dual-read migration and compatibility report |
| More projection fields slow backend | index hot joins and benchmark projection generation |
| Swap/move UX lands before audit trail | keep actions hidden until backend event logging exists |
| DataGrid refresh behavior differs for custom renderers | row patch first, cell refresh after renderer contract tests |

## Immediate Next Step

Start with Slice 8.

Slice 8 should introduce a runtime test patch stream so signal test status/value/timestamps arrive as row/cell patches instead of full allocation projection refreshes.
