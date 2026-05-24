# Signal List and Allocation Slice Plan

Status: working migration checklist
Last reviewed: 2026-05-24

## Purpose

This document is the slice-by-slice plan for rebuilding the signal list and allocation architecture without carrying the old reactive grid design forward.

Update this file as each slice starts and finishes:

- `[ ]` not started
- `[~]` in progress
- `[x]` done
- `[!]` blocked or needs a decision

## Target Architecture

```text
Backend durable state
  signals / devices / channels / allocations / test jobs
        |
        | initial load
        v
Flat signal allocation projection
        |
        | set once / reload only for recovery
        v
Affino DataGrid client row model
        |
        | allocation, device, runtime patches
        v
Patch queue -> rows.patch / refreshCells
```

Ownership rules:

- Backend owns allocation truth, uniqueness, compatibility checks, job execution, and technical execution validation.
- DataGrid owns row rendering, viewport, selection, editing, scroll position, and cell refresh.
- Frontend feature layer owns operator workflows and maps backend patches to DataGrid patches.
- Pinia is not the hot-path owner of 20,000 grid rows. It can hold workspace, job, session, dirty/save, and coarse cached state.
- Full projection reload is a recovery path, not the normal allocation or runtime update path.

Execution semantics for the current product direction:

- Signal sheet aliases are live working names for physical bindings.
- Test execution resolves the current binding at the moment of execution.
- Sheet-level revision tokens must not block command/test execution.
- Missing binding, offline device, missing channel, or incompatible channel mode is handled per signal as skipped/failed technical execution state.
- If report/evidence requirements grow later, capture the actual signal/allocation/channel evidence used by each step instead of blocking on sheet-level metadata drift.

## Completed Baseline

- `[x]` Flat allocation projection includes row identity, allocation state, health fields, device/channel fields, and tested timestamp.
- `[x]` Allocation is stored as explicit signal-to-channel records, not embedded only inside signal rows.
- `[x]` Backend has assign, unassign, reassign, swap, bulk update, and auto-allocation paths.
- `[x]` Mandatory auto-allocation preview flow was removed from the normal path.
- `[x]` Allocation jobs return changed row patch payloads.
- `[x]` Test-run execution no longer fails whole jobs because of signal-sheet revision drift.
- `[x]` Runtime tested-at updates can flow as patches.
- `[x]` Allocation quick-filter buttons were removed from the DataGrid toolbar.
- `[x]` Channel picker uses explicit apply/swap behavior and avoids click-to-swap for occupied channels.
- `[x]` First grid hot-path cleanup is in place: SignalPage uses a grid row model wrapper and DataGrid `rows.patch` instead of replacing the row array for allocation job patches.

## Main Gaps

- Pinia still exposes `allocationRows` as a Vue `ref<SignalAllocationRow[]>`; `SignalsPage.vue` now bridges it into a non-reactive projection cache, but other app areas can still drift back into deep reactive row ownership.
- Store patching still updates array slots for allocation changes; `SignalsPage.vue` applies those updates to the non-reactive cache, but the store itself is still legacy-shaped.
- Runtime state is only partially separated from static projection rows.
- Allocation, test, and device patch ingress has a sequenced WebSocket contract, but backend producers still need to migrate from action-specific payloads.
- Sort/filter recompute policy is explicit for current allocation/runtime columns; future device/test fields still need policy entries when introduced.
- A pure frontend 20,000-row benchmark harness now covers projection, patch queue, selection, and channel-owner lookup; real browser scroll/viewport proof is still pending.
- Allocation summary event history and signal test-run step evidence exist; report/controller-log comparison wiring is still incomplete.
- Full projection reload is now policy-gated for initial load, workspace/import changes, reconnect gaps, unknown rows, and operator refresh; structural signal-list edits still use explicit reload.

## Slice Plan

### Slice 0 - Working Plan Document

Status: `[x]`

Goal:

- Create this checklist as the single working plan for the migration.

Deliverables:

- `docs/architecture/signal-list-slice-plan.md`

Validation:

- Documentation review.

Rollback:

- Revert this document only.

### Slice 1 - Grid-Owned Row Patch Baseline

Status: `[x]`

Goal:

- Stop using Vue full-array replacement as the normal allocation update path.
- Route allocation job updates through DataGrid row patch APIs.

Implemented:

- `useSignalGridPatchQueue` targets `api.rows.patch`.
- `useSignalGridRowModel` owns the shallow initial row list and row-id cache.
- Allocation job completions patch DataGrid first, then sync Pinia in background chunks.
- Backend allocation jobs return lightweight `changed_row_patches`.

Validation:

- Frontend type-check.
- Focused Vitest coverage for patch queue and row model.
- Backend syntax check for allocation runner.

Remaining manual checks:

- Large auto-allocation keeps scroll responsive.
- Changed allocation cells appear before job completion.
- Assign/unassign buttons do not remain stuck after completion.

Rollback:

- Re-enable full projection reload after allocation jobs.

### Slice 2 - Projection Mapper Boundary

Status: `[x]`

Goal:

- Put all backend projection-to-grid-row mapping behind one tested boundary.
- Prevent ad hoc `createGridRow(...)` calls from spreading through page code.

Backend:

- Keep returning a flat projection.
- Keep changed row patches small and compatible with the projection row shape.

Frontend:

- Add a projection mapper module for:
  - server projection row -> static grid row;
  - server changed row patch -> grid row patch;
  - runtime overlay -> grid cell patch.
- Move source-column expansion out of `SignalsPage.vue` where practical.

Tests:

- Mapper tests for assigned, unassigned, offline, invalid, source metadata, and tested-at overlay.
- Frontend type-check.

Validated 2026-05-24:

- `pnpm --dir frontend type-check`
- `pnpm --dir frontend test src/pages/signals/utils/signalGridProjection.test.ts src/pages/signals/composables/useSignalGridPatchQueue.test.ts src/pages/signals/composables/useSignalGridRowModel.test.ts src/pages/signals/utils/rowSelection.test.ts src/pages/signals/utils/allocationHealth.test.ts src/utils/signalRuntimeMapping.test.ts`

Rollback:

- Keep existing page-level mapping functions.

### Slice 3 - Non-Reactive Projection Cache

Status: `[x]`

Goal:

- Replace hot-path `allocationRows` usage with a non-reactive projection cache plus coarse reactive version counters.

Frontend:

- Introduce a projection model with:
  - `rowsBySignalId`;
  - `rowsByRowId`;
  - `rowOrder`;
  - `ownerSignalIdByChannelId`;
  - `projectionVersion`;
  - `changedSignalIds`.
- Pinia may expose only coarse state and commands, not a deeply reactive full row array for grid rendering.
- `SignalsPage.vue` reads indexed projection data for selection, picker ownership, and summaries.

Tests:

- Projection cache patch tests.
- Channel ownership index tests.
- Selection helper tests against projection cache.

Validated 2026-05-24:

- `pnpm --dir frontend type-check`
- `pnpm --dir frontend test src/pages/signals/utils/signalAllocationProjectionCache.test.ts src/pages/signals/utils/signalGridProjection.test.ts src/pages/signals/composables/useSignalGridPatchQueue.test.ts src/pages/signals/composables/useSignalGridRowModel.test.ts src/pages/signals/utils/rowSelection.test.ts src/pages/signals/utils/allocationHealth.test.ts src/utils/signalRuntimeMapping.test.ts`

Rollback:

- Keep the current Pinia `allocationRows` path while the projection cache is behind the page layer.

### Slice 4 - Unified Patch Ingress

Status: `[x]`

Goal:

- Make allocation, runtime test, and device/channel health updates enter the frontend through one patch queue contract.

Frontend:

- Add a patch ingress module that accepts:
  - allocation row patches;
  - runtime tested/status/value patches;
  - device/channel health patches.
- Coalesce by `rowId` and column list.
- Apply DataGrid patches on animation frames.
- Update non-reactive projection/runtime caches before DataGrid patching.

Backend:

- Keep REST job responses for explicit actions.
- Prepare WebSocket/SSE event shape for future push patches.

Tests:

- Coalescing tests.
- Ordering tests per row.
- Unknown row handling test.

Validated 2026-05-24:

- `pnpm --dir frontend type-check`
- `pnpm --dir frontend test src/pages/signals/utils/signalGridPatchIngress.test.ts src/pages/signals/utils/signalAllocationProjectionCache.test.ts src/pages/signals/utils/signalGridProjection.test.ts src/pages/signals/composables/useSignalGridPatchQueue.test.ts src/pages/signals/composables/useSignalGridRowModel.test.ts src/pages/signals/utils/rowSelection.test.ts src/pages/signals/utils/allocationHealth.test.ts src/utils/signalRuntimeMapping.test.ts`

Rollback:

- Keep direct action-specific patch calls.

### Slice 5 - Runtime State Split

Status: `[x]`

Goal:

- Separate volatile runtime test state from static signal/allocation projection wherever sort/filter does not require static row mutation.

Frontend:

- Runtime cache stores:
  - tested timestamp;
  - current test status;
  - current value;
  - last update timestamp;
  - skip/fail reason.
- Display-only runtime cells use `refreshCells`.
- Sort/filter-participating runtime columns use row patches with explicit recompute policy.

Tests:

- Visible row refresh test.
- Non-visible row runtime update test.
- Runtime patch does not replace `rows.value`.

Validated 2026-05-24:

- `pnpm --dir frontend type-check`
- `pnpm --dir frontend test src/pages/signals/utils/signalRuntimeStateCache.test.ts src/pages/signals/utils/signalGridPatchIngress.test.ts src/pages/signals/utils/signalAllocationProjectionCache.test.ts src/pages/signals/utils/signalGridProjection.test.ts src/pages/signals/utils/runtimeProjection.test.ts src/pages/signals/composables/useSignalGridPatchQueue.test.ts src/pages/signals/composables/useSignalGridRowModel.test.ts src/pages/signals/utils/rowSelection.test.ts src/pages/signals/utils/allocationHealth.test.ts src/utils/signalRuntimeMapping.test.ts`

Notes:

- `tested_at` remains a row patch because it is a sortable/filterable grid column.
- Display-only runtime fields can use the existing `refreshSignalCells` ingress when added.

Rollback:

- Continue overlaying runtime fields into grid row patches.

### Slice 6 - Sort/Filter Recompute Policy

Status: `[x]`

Goal:

- Make live-update projection behavior explicit.

Frontend:

- Define per-column patch policy:
  - display-only refresh;
  - row patch without sort/filter recompute;
  - row patch with filter recompute;
  - row patch with sort/filter recompute.
- During high-volume test runs, avoid automatic full projection recompute unless the user explicitly depends on it.

Tests:

- Patched non-filter field does not move rows.
- Patched filter field updates membership only when policy says so.
- Sorting behavior is predictable under live updates.

Validated 2026-05-24:

- `pnpm --dir frontend type-check`
- `pnpm --dir frontend test src/pages/signals/utils/signalGridPatchPolicy.test.ts src/pages/signals/utils/signalRuntimeStateCache.test.ts src/pages/signals/utils/signalGridPatchIngress.test.ts src/pages/signals/utils/signalAllocationProjectionCache.test.ts src/pages/signals/utils/signalGridProjection.test.ts src/pages/signals/utils/runtimeProjection.test.ts src/pages/signals/composables/useSignalGridPatchQueue.test.ts src/pages/signals/composables/useSignalGridRowModel.test.ts src/pages/signals/utils/rowSelection.test.ts src/pages/signals/utils/allocationHealth.test.ts src/utils/signalRuntimeMapping.test.ts`

Notes:

- Runtime `tested_at` patches are row patches with sort/filter/group recompute disabled by default.
- Single-row allocation status/health patches recompute sort/filter/group by policy.
- Bulk allocation job patches explicitly override recompute to false to keep large jobs responsive.

Rollback:

- Default to no recompute and provide manual refresh/reload.

### Slice 7 - Backend Patch Event Contract

Status: `[x]`

Goal:

- Move live allocation/runtime/device patch delivery toward sequenced WebSocket/SSE events.

Backend:

- Add event shape:

```ts
type SignalRowsPatchedEvent = {
  event: "signal_rows_patched"
  workspaceId: number
  sequence: number
  source: "allocation" | "test_runtime" | "device_health"
  patches: Array<{
    rowId: string
    signalId: number
    changes: Record<string, unknown>
    columns?: string[]
  }>
  requiresFullReload?: boolean
}
```

Frontend:

- Detect sequence gaps.
- Use full reload only on gap, unknown workspace, or explicit `requiresFullReload`.

Tests:

- Event serialization.
- Sequence gap reload trigger.
- Duplicate/old event ignored.

Implemented:

- Backend schema for `signal_rows_patched` with `workspace_id`, `sequence`, `source`, row patches, and recovery reload flag.
- Frontend WebSocket types and handler routing into `signalRowsPatchStore`.
- Frontend store detects active-workspace events, sequence gaps, duplicate/old events, and explicit full-reload recovery.
- `SignalsPage.vue` consumes the sequenced event through the existing grid patch ingress and reloads only for recovery conditions.

Validated 2026-05-24:

- `pnpm --dir frontend type-check`
- `pnpm --dir frontend test src/stores/signalRowsPatchStore.test.ts src/pages/signals/utils/signalGridPatchIngress.test.ts`
- `uv run python -m py_compile app/schemas/ws/events.py tests/workers/test_signal_allocation_runner_results.py`
- `git diff --check`

Notes:

- Backend producers are not switched to emit `signal_rows_patched` yet; REST job-result patching remains active.
- `uv run pytest tests/workers/test_signal_allocation_runner_results.py` could not run because `pytest` is not installed in the current backend environment.

Rollback:

- Continue REST job-result patching only.

### Slice 8 - Allocation Event History

Status: `[x]`

Goal:

- Make allocation changes auditable without making the UI slow.

Backend:

- Add append-only allocation events for assign, unassign, reassign, swap, auto allocation, and bulk update.
- Keep event writes inside allocation transactions.
- Do not require the frontend to load events for normal grid rendering.

Tests:

- Event written for each mutation type.
- Failed mutation writes no success event.
- Bulk events capture requested, changed, skipped, and rejected counts.

Implemented:

- Added append-only `signal_allocation_events` persistence table and SQLAlchemy model.
- Synchronous assign, unassign, reassign, swap, bulk update, and auto-allocation write history rows before transaction commit.
- Async allocation worker records `auto_allocate` and `bulk_update` summary events in the same database transaction as allocation changes.
- Bulk and auto events store requested/changed/skipped/rejected counters and compact changed-signal payloads; normal grid projection loading does not read event history.
- `update_allocations` now reports actual changed signal ids so no-op bulk entries do not force changed row patches.

Validated 2026-05-24:

- `uv run python -m py_compile app/models/signal_sheet.py app/models/__init__.py app/api/v1/signal_sheet/repository.py app/services/signal_sheet_write_service.py app/workers/signal_allocation_runner.py tests/services/test_signal_sheet_write_service_allocation_actions.py alembic/versions/f1a2b3c4d5e7_add_signal_allocation_events.py`
- `git diff --check`

Notes:

- `uv run pytest tests/services/test_signal_sheet_write_service_allocation_actions.py` could not run because `pytest` is not installed in the current backend environment.

Rollback:

- Disable event writes while keeping current allocation table state.

### Slice 9 - Test Execution Evidence

Status: `[x]`

Goal:

- Keep live alias semantics but persist what was actually executed.

Backend:

- For each step, capture:
  - signal id;
  - current allocation id if available;
  - channel id;
  - unit id;
  - command payload;
  - ack/result state;
  - skipped/failure reason;
  - timestamp.
- Do not block a job because a sheet-level metadata token changed.

Frontend:

- Surface per-signal skipped/failure reasons in grid/report flows.

Tests:

- Unbound signal is skipped per signal.
- Offline/missing/incompatible channel is skipped per signal.
- Successful command records binding/channel evidence.

Implemented:

- Added append-only `signal_test_run_step_evidence` persistence table and SQLAlchemy model.
- Signal test-run worker records one evidence row per processed signal alias.
- Evidence captures job id, attempt id/no, order index, signal id, allocation id, channel/device/unit fields, command payload, result state, skip reason, and tested timestamp.
- Test execution still resolves aliases to current bindings at execution time; no signal-sheet revision guard was added.
- Skipped rows record per-signal reasons for missing binding, invalid binding, incompatible channel mode, and offline unit.
- Successful DO/AO command enqueue records the actual command payload and current binding/channel evidence.

Validated 2026-05-24:

- `uv run python -m py_compile app/models/signal_sheet.py app/models/__init__.py app/api/v1/signal_sheet/repository.py app/workers/signal_test_run_runner.py tests/workers/test_signal_allocation_runner_results.py alembic/versions/f2a3b4c5d6e8_add_signal_test_run_step_evidence.py`
- `git diff --check`

Notes:

- `uv run pytest tests/workers/test_signal_allocation_runner_results.py` could not run because `pytest` is not installed in the current backend environment.
- Report/UI surfacing of persisted evidence is not implemented in this slice.

Rollback:

- Keep current per-signal skip counts without evidence detail.

### Slice 10 - Full Reload Recovery Path Cleanup

Status: `[x]`

Goal:

- Make reload paths explicit recovery tools, not normal update behavior.

Frontend:

- Remove silent reloads after normal allocation/test updates.
- Keep explicit reload for:
  - initial load;
  - workspace switch;
  - import;
  - reconnect gap;
  - unknown row patch;
  - operator manual refresh.

Tests:

- Allocation job completion does not call full allocation reload.
- Runtime patch burst does not call full allocation reload.
- Unknown row patch requests recovery reload.

Implemented:

- Added an explicit static-refresh policy for initial load, workspace switch, import, operator manual refresh, reconnect gap, and unknown-row recovery.
- `SignalsPage.vue` now routes patch-stream full reloads through named recovery reasons instead of silent `refreshSignalsStatic()` calls.
- Runtime and allocation patch handlers return missing signal ids, so unknown-row recovery is explicit while normal patch bursts remain reload-free.
- Pinia allocation patching no longer replaces the loaded allocation projection with partial server patch rows when a normal patch references unknown rows.

Validated 2026-05-24:

- `pnpm --dir frontend type-check`
- `pnpm --dir frontend test src/pages/signals/utils/signalStaticRefreshPolicy.test.ts src/stores/signalSheetStore.test.ts src/stores/signalRowsPatchStore.test.ts src/pages/signals/utils/signalGridPatchIngress.test.ts`
- `git diff --check`

Rollback:

- Restore reload fallback for affected mutation.

### Slice 11 - 20,000 Row Benchmark Harness

Status: `[x]`

Goal:

- Prove the target workflow before adding more features.

Benchmarks:

- Initial 20,000-row projection load.
- Patch one visible row.
- Patch one non-visible row.
- Apply 5,000 allocation row patches.
- Apply 5,000 runtime patches.
- Scroll during patch burst.
- Select all visible/all filtered rows.
- Open channel picker from an allocated row.

Acceptance targets:

- No full reload during normal patch paths.
- Scroll remains responsive during large patch bursts.
- Selection survives patching.
- No blank viewport during scroll.
- Patch queue does not grow unbounded.

Implemented:

- Added `runSignalListPerformanceHarness()` for the signal-list frontend path.
- Harness builds a 20,000-row flat allocation projection and initial grid row model.
- Harness applies one visible row patch, one non-visible row patch, 5,000 allocation patches, and 5,000 runtime tested-at patches.
- Harness asserts normal allocation/runtime patch paths do not request static reload while unknown rows still request recovery reload.
- Harness verifies queue coalescing stays bounded to the 5,000 affected row ids and defers grid patch calls until flush.
- Harness covers select-all, visible selection, and channel-owner lookup used by the channel picker.

Validated 2026-05-24:

- `pnpm --dir frontend type-check`
- `pnpm --dir frontend test src/pages/signals/utils/signalListPerformanceHarness.test.ts src/pages/signals/utils/signalStaticRefreshPolicy.test.ts src/pages/signals/utils/signalGridPatchIngress.test.ts src/pages/signals/composables/useSignalGridPatchQueue.test.ts`
- `git diff --check`

Notes:

- This is a deterministic unit-level performance contract, not a browser rendering benchmark.
- Real DataGrid scroll smoothness, blank viewport behavior, and overlay alignment still need browser/manual or Playwright coverage before claiming end-to-end UI performance.

Rollback:

- If client-side mode fails measured targets, evaluate server-backed row model with explicit viewport/cache contracts.

### Slice 12 - Legacy Cleanup

Status: `[~]`

Goal:

- Delete old paths once the new architecture is proven.

Cleanup:

- Remove unused preview state and docs.
- Remove old full-array grid computed paths.
- Remove duplicate allocation mutation helpers.
- Remove stale revision/snapshot guard comments.
- Remove temporary fallback code that benchmarks prove unnecessary.

Tests:

- Full frontend type-check.
- Focused backend allocation/test-run tests.
- Focused frontend signal page tests.
- 20,000-row benchmark repeat.

Implemented so far:

- Removed the legacy partial-projection replacement fallback from Pinia allocation patching.
- Updated migration docs that still described full reload fallback as the normal patch safety path.
- Removed the stale test-run fixture that still supplied `signal_sheet_revision` metadata to a queued run payload.
- Updated migration docs to state that queued run payloads do not carry sheet-level revision metadata.
- Removed the redundant `skipMissing` allocation patch option; unknown rows now always use the same explicit skip/recovery path.
- Removed unused legacy signal sidebar/live-panel components and a dead channel-id helper from the signal sheet store.
- Removed unused `signalSheetStore.bootstrap()` and `getAllocationOwnerSignalId()` APIs.
- Updated older migration notes that still described grid-row and runtime-patch fallbacks as normal behavior.
- Removed the inactive `Create switchgear` header stub that was always disabled from `SignalsPage.vue`.
- Removed unused `createSignalGridRowPatches()` export from the projection mapper.
- Removed unused duplicate allocation helpers `setAllocation()` and direct store-level `autoAllocate()`.
- Removed the unused frontend direct auto-allocation API wrapper and response type; normal UI auto-allocation stays job-based.
- Rechecked mandatory allocation preview cleanup; no `previewId`/preview session state remains in the normal frontend/backend allocation flow.
- Fixed the allocation job completion patch path to call DataGrid `rows.patchRows` and preserve explicit `null` fields in bulk unassign row patches.
- Converted the Pinia allocation row cache to a shallow snapshot so bulk job completion sync does not deep-track thousands of row objects after the grid has already patched.
- Narrowed grid row patch payloads to requested columns and suppressed grid state persistence events during bulk job patch replay.
- Removed mass `rows.patchRows` replay on allocation job completion; completion now updates the allocation projection cache and refreshes only visible cells.
- Removed the unused public `signalSheetStore.applyAllocationRowsPatch()` helper and the obsolete `rows.patch` DataGrid API fallback.

Validated 2026-05-24:

- `pnpm --dir frontend type-check`
- `pnpm --dir frontend test src/stores/signalSheetStore.test.ts src/pages/signals/composables/useSignalGridPatchQueue.test.ts src/pages/signals/composables/useSignalGridRowModel.test.ts src/pages/signals/utils/signalGridPatchIngress.test.ts src/pages/signals/utils/signalGridProjection.test.ts src/pages/signals/utils/signalAllocationJobResult.test.ts src/pages/signals/utils/signalListPerformanceHarness.test.ts`
- `uv run python -m py_compile app/workers/signal_test_run_runner.py tests/workers/test_signal_allocation_runner_results.py`
- `git diff --check`

Notes:

- `uv run pytest tests/workers/test_signal_allocation_runner_results.py` could not run because `pytest` is not installed in the current backend environment.

Remaining:

- Remove old full-array grid paths once browser/manual proof confirms they are no longer needed for initial load or recovery.

Rollback:

- Revert only the cleanup commit, not the new architecture slices.

## Update Protocol

For each future slice:

1. Change its status to `[~]`.
2. Implement the smallest coherent change.
3. Add or update focused tests.
4. Record validation commands in the slice.
5. Change status to `[x]` only after validation passes or blocked work is clearly documented.
6. Add new gaps to `Main Gaps` instead of hiding them.

## Risks

| Risk | Mitigation |
| --- | --- |
| Patch stream drift | sequence numbers, unknown-row recovery reload |
| Live sort/filter surprises | explicit per-column recompute policy |
| Vue deep reactivity returns to hot path | non-reactive projection cache and row-model tests |
| Grid patch API misuse | keep app wrapper around `api.rows.patch` and `refreshCellsByRowKeys` |
| Bulk updates still block UI | chunk patches and store sync; benchmark 5,000+ patches |
| Runtime evidence remains too weak | persist per-step binding/channel evidence without sheet-level execution guards |
| Reload fallback hides regressions | tests assert no reload in normal mutation paths |

## Next Slice

Continue Slice 12: Legacy Cleanup.

Reason:

- The pure 20,000-row projection and patch contract is now covered.
- The first cleanup removed the partial projection replacement fallback; remaining cleanup should stay small and separately validated.
