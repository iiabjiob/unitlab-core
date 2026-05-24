# Signal List and Allocation Migration Plan

Status: Slice 10 simplification applied, binding-level execution checks next
Last reviewed: 2026-05-24

## Scope

This note consolidates the signal-list, allocation, live-test, DataGrid, and allocation-UX audits into a staged migration plan.

The goal is a high-performance FAT signal-list editor that stays responsive at about 20,000 rows while backend services remain the source of truth for allocation state, technical execution checks, command dispatch, and local run telemetry.

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
- auto allocation and bulk unassign apply immediately and return changed row patches plus skipped/rejected summaries.

Gaps:

- conflict, invalid type, missing, stale, and offline health are visible in grid columns; dedicated resolution workflows are still pending.
- durable allocation event history is still pending.

### Live Test Updates

Implemented:

- auto-test jobs run in a backend worker.
- job progress is published over WebSocket.
- tested timestamps can be sent as per-signal patches inside job event result payloads.
- test-run workers now also emit dedicated `signal_test_runtime_patch` WebSocket events for tested timestamp patches.
- frontend applies test runtime patches through the realtime tested-at store and existing DataGrid row/cell patch queue.

Gaps:

- runtime test fields are mixed into signal-list row data.
- frontend updates still cause broad computed-row churn.
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
  signals / devices / channels / allocations / test runs
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
| Auto allocation | immediate apply with skipped/rejected summary | richer compatibility warnings and apply parity tests |
| Runtime test state | mixed into row data | patch stream, optionally separate runtime store |
| Allocation health | projection fields + grid badges/filters | resolution workflows and event history |
| Conflict UX | mostly hidden | visible conflict state and resolution actions |
| Execution consistency | live aliases and current bindings | resolve current bindings at execution; capture binding/channel evidence per step |

## Proposed Data Model Changes

### Keep

- `signals`
- `signal_allocations`
- `devices`
- `channels`
- current uniqueness constraints on active allocation ownership.

### Add Or Evolve

Do not add durable signal-list revision tables for this migration path. The signal sheet is a live semantic alias layer. Test execution should validate and record the current binding/channel evidence it actually used, not block on a sheet-level token.

#### `allocations`

Either evolve `signal_allocations` or add a versioned replacement.

Required fields:

- `id`
- `workspace_id`
- `signal_id`
- `channel_id`
- `status`: active, superseded, removed
- `allocation_meta`
- `created_by`
- `created_at`
- `updated_at`

Constraints:

- one active allocation per signal;
- one active allocation per channel per workspace;
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

- signal id;
- allocation id;
- channel id;
- unit id;
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
  signalId: number
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
- `PUT /signal-allocations`
- `POST /signal-allocations/auto`

Response shape:

```ts
type AllocationMutationResponse = {
  workspaceId: number
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
  sequence: number
  patches: Array<{
    rowId: string
    signalId: number
    changes: Record<string, unknown>
    columns?: string[]
  }>
}
```

Frontend must reload projection when:

- sequence gap is detected;
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

- DataGrid column filters/search for unassigned, assigned, conflicts, invalid type, offline/missing device, signal type, cabinet/source column, and device/unit;
- channel picker states: free, current, occupied, incompatible, offline, missing;
- occupied channel owner display;
- explicit swap/move actions;
- auto-allocation immediate apply;
- bulk allocation immediate apply;
- structured skipped/rejected/conflict result display after bulk operations.

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
- unknown/missing changed rows are treated as explicit recovery cases, not as partial projection replacement.

Tests:

- allocation worker serialization test for terminal `changed_rows` payload;
- frontend type check;
- normal allocation/test patch paths do not trigger full reload.

Rollback:

- restore explicit recovery reload for the affected mutation if patch delivery cannot be trusted.

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

- make conflict/invalid/offline/missing states visible in the grid and filterable through the grid's normal filtering.

Backend:

- compute health fields in projection.

Frontend:

- add status badges and keep allocation fields available to the grid's normal filter/search controls.
- implemented signal-grid `Allocation` and `Health` columns backed by flat projection fields.
- removed the dedicated allocation quick-filter toolbar buttons; channel occupancy lookup still uses the full projection so filtered-out owners are not treated as free.

Tests:

- projection fixtures;
- DataGrid filter tests;
- visual/manual verification for badge states.
- current frontend coverage includes pure allocation health helper tests and existing patch queue tests.

Rollback:

- hide badges; projection fields remain harmless.

### Slice 7 - Immediate Auto/Bulk Allocation Apply

Status: done.

Goal:

- apply operator-requested allocation changes immediately while reporting changed, skipped, and rejected rows.

Backend:

- removed mandatory dry-run preview endpoints from the normal allocation flow.
- implemented immediate `POST /workspaces/{workspace_id}/signal-allocations/auto` responses with changed rows, skipped items, and rejected items.
- implemented immediate `PUT /workspaces/{workspace_id}/signal-allocations` responses with changed rows and rejected items.

Frontend:

- selected auto-assign and bulk unassign apply immediately.
- frontend patches the grid from `changed_rows`.
- toast summaries show requested, changed, and skipped counts.

Tests:

- immediate apply changed-row patching;
- no overwrite without explicit option;
- large selection performance.
- current backend coverage verifies immediate auto assignment and skipped-item reporting.
- current frontend validation covers type contracts; browser visual verification remains manual.

Rollback:

- keep old auto allocation button behind fallback.

### Slice 8 - Runtime Test Patch Stream

Status: done.

Goal:

- avoid full row churn during test runs.

Backend:

- emit `signal_test_runtime_patch` events with job id, workspace id, patch type, and changed tested-at values by signal id.
- keep `tested_at_patch` in job result payloads as a compatibility fallback.

Frontend:

- batch patches in rAF.
- patch only status/value/timestamp cells.
- route `signal_test_runtime_patch` events directly into `testedAtRealtimeStore`.
- keep terminal job handling able to flush pending tested-at patches into the persisted allocation row store even when the terminal job event has no patch payload.

Tests:

- WebSocket patch ordering;
- reconnect gap recovery;
- 1,000 patch burst while scrolling.
- current backend coverage verifies runtime patch event serialization.
- current frontend validation covers WS type contracts and existing grid patch queue tests.

Rollback:

- continue using existing job event result patches and explicit recovery reload.

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

Implemented:

- `tested_at` runtime patches remain in `testedAtRealtimeStore` and are overlaid only when building grid patches, reports, and exports.
- Signal selection, channel ownership maps, and allocation picker state now read the static allocation projection instead of a runtime-mapped row array.
- Live `tested_at` patches still target the affected grid row/cell through the existing patch queue; non-visible rows update the runtime store without forcing allocation maps to rebuild.
- Summary counts may still scan the static row array to show live tested totals; this should be measured in the 20,000-row benchmark slice before adding a separate aggregate counter.

Status: done.

Rollback:

- fold runtime fields back into row patches.

### Slice 10 - Live Alias Execution Semantics

Goal:

- keep the signal sheet as a live alias layer while making execution evidence explicit.

Backend:

- resolve signal aliases to current allocations when the worker executes.
- validate missing bindings, offline devices, missing channels, and incompatible channel modes per signal.
- capture the signal/allocation/channel evidence used for each executed or skipped step.

Implemented:

- queued test-run jobs no longer include or enforce a sheet-level revision token.
- the worker resolves each selected signal against its current allocation row at the moment that signal is executed.
- missing rows, invalid bindings, incompatible channel modes, and offline units are counted as per-signal skips.

Frontend:

- show binding health and per-signal skip/failure reasons.

Tests:

- queued run payloads do not carry sheet-level revision metadata;
- missing/unbound signal handling remains per-signal;
- report/test evidence includes the binding/channel actually used.

Status: partial. Binding evidence persistence and report reconstruction remain pending.

Rollback:

- restore full projection reload as a recovery path if binding-level patching drifts.

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
| Patch stream drift | sequence numbers and explicit recovery reload |
| Live sort/filter row movement | freeze projection during active test run or require explicit reapply |
| Live alias rebinding changes queued-run behavior | capture allocation/channel evidence at step execution and surface skipped signals clearly |
| More projection fields slow backend | index hot joins and benchmark projection generation |
| Swap/move UX lands before audit trail | keep actions hidden until backend event logging exists |
| DataGrid refresh behavior differs for custom renderers | row patch first, cell refresh after renderer contract tests |

## Immediate Next Step

Continue with binding-level execution telemetry.

The next slice should persist the allocation/channel used for each local UnitLab command and keep queued execution tied to current resolvable bindings, not sheet-level revision metadata.
