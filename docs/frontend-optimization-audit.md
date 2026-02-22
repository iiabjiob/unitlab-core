# Frontend Optimization Audit & Implementation Pipeline

## Goal
Make the frontend predictable, fast, and maintainable:
- minimize broad `watch` usage and cascading reactive loops
- correct data loading ownership (boot/runtime/page/detail)
- reduce duplicate listeners / duplicate fetches
- split oversized stores by responsibility
- preserve current UX and visual design

## What Is Already Good
- Runtime boot is idempotent (`booted` + `bootInFlight`) in `frontend/src/boot/runtime.ts`.
- Store bootstrap dedupe exists via `runStoreBootstrap(...)`.
- WebSocket reconnect and queueing are already structured.
- Realtime scoping (`realtimeScopeStore`) is a strong base for page-specific subscriptions.
- Signals/test-run realtime updates already use some throttling/coalescing patterns.

## Current Architectural Leaks
1. Global listeners registered without one-time guard.
- `frontend/src/boot/webSocket.ts` adds `online` / `visibilitychange` / `focus` / `pageshow` handlers on each call.
- Risk: duplicate reconnect nudges and hidden behavior drift after HMR/re-init.

2. Duplicated viewport/resize listeners.
- `frontend/src/composables/useViewport.ts` creates one `resize` listener per component.
- `frontend/src/components/layout/AppLayout.vue` also tracks viewport width independently.
- Risk: unnecessary listener count and noisy reactivity.

3. Polling/system health orchestration mixed into layout component.
- `AppLayout.vue` owns timers + visibility listeners + refresh calls.
- Risk: UI layer managing data lifecycle, hard to reason/test.

4. Heavy watch orchestration in page components (`signals` hotspot).
- `frontend/src/pages/signals/components/AllocationEditor.vue` contains multiple watch-based triggers for refresh/scope sync/query-driven UI.
- Risk: cascade updates, hard-to-debug sequencing, expensive recompute under realtime load.

5. Oversized stores with mixed responsibilities.
- `channelStore.ts` mixes catalog loading, runtime states, diagnostics, command lifecycle, and UI-facing command state.
- `switchgearStore.ts` mixes persisted model, runtime derived state, and command orchestration.
- Risk: accidental coupling and over-recomputation.

6. DataGrid watcher density (`UiAffinoDataGrid.vue`).
- Many watchers are valid (UI sync), but persistence/layout/selection watchers can be batched into fewer scheduler paths.
- Risk: micro-lag and maintenance complexity.

## Optimization Principles (Project Rules)
1. `computed` is for pure derivation only (no network side effects).
2. `watch` should be narrow and preferably event-driven/scheduled, not broad tuple catch-alls.
3. Network loading lives in stores/services, not layout/page render code.
4. Realtime updates should patch overlays, not rebuild heavy base datasets unless structure changed.
5. Boot/runtime/page/detail loading ownership must be explicit.

## Loading Model (Correct Data Ownership)
### Preload
Purpose: UI shell readiness only.
- Theme
- UI preferences
- Selection restore
- WebSocket connect (and reconnect hooks)

### Runtime Boot (global lightweight catalogs)
Purpose: app-wide baseline data after workspace is known.
- Workspaces
- Devices (basic list)
- Switchgears list
- Sequences list

### Route/Page Load (page snapshot)
Purpose: page-specific payloads only when route is opened.
- `/signals`: signal sheet + allocations + grid state hydration
- `/devices/:id`: per-device channels + states/diagnostics request
- `/switchgears/:id`: switchgear detail, bindings, runtime subscriptions

### Detail/Lazy Load
Purpose: demand-driven heavy data.
- Device channels for units referenced by visible signal allocations
- Logs/history/details panels when opened

## Phased Pipeline

### P0. Stability & Listener Hygiene (quick wins, low risk)
1. Add one-time guard to `bootWebSocket()` so reconnect listeners are registered once.
2. Replace inline listener lambdas with named handlers.
3. Convert `useViewport()` to shared singleton source (same public API) to avoid one listener per consumer.
4. Remove duplicated mobile-width listener from `AppLayout.vue`; read from shared viewport source.
5. Keep `AppLayout` responsible only for layout selection UI logic.

Success criteria:
- repeated boot/init does not multiply reconnect hooks
- viewport consumers share one global resize observer/listener
- no behavior changes in layout switching

### P1. Data Loading Orchestration (highest ROI)
1. Standardize store APIs:
- `ensureLoaded({ workspaceId, force?, ttlMs? })`
- `refresh({ workspaceId, force? })`
- `invalidate(workspaceId?)`
2. Add in-flight dedupe + TTL in stores (not components).
3. Move page refresh orchestration into page-specific composables/services where possible.
4. Support cancellation (`AbortController` or equivalent token) for route/workspace transitions.
5. Keep components declarative: `ensurePageData()` + `subscribeRealtime()` only.

Success criteria:
- no duplicate fetches on rapid route/workspace changes
- stale async responses do not overwrite current page state
- logs become predictable (single load cycle per route entry)

### P2. Watch Reduction on Signals Page (`AllocationEditor.vue`)
1. Group watchers into 3 schedulers:
- data refresh scheduler
- realtime scope reconcile scheduler
- local UI persistence scheduler
2. Collapse multiple scope-related watchers into `requestScopeReconcile(reason)`.
3. Replace revision-to-revision watch chains with explicit event calls from mutation handlers where possible.
4. Keep realtime `tested_at`/status updates as overlay patches (avoid full grid row rebuilds).
5. Cache large lookup maps by revision and reuse.

Success criteria:
- fewer watcher cascades under WS traffic
- no visible lag on signals page during realtime updates
- simpler sequencing for workspace/route refresh

### P3. Store Decomposition (remove “god files”)
1. Split channel concerns:
- `channelCatalogStore`
- `channelRuntimeStore`
- `channelCommandStore`
- logs remain separate or hardened boundary
2. Split switchgear concerns:
- persisted switchgear model/bindings
- runtime derived position/status
- command orchestration
3. Keep `wsHandler.ts` as message router only.
4. Define explicit source-of-truth:
- WS state for runtime values
- commands send intent only

Success criteria:
- smaller files, clearer tests, less accidental reactive coupling

### P4. DataGrid Internal Hygiene (without UX regressions)
1. Batch persistence-related watchers into one scheduler path.
2. Batch layout invalidations into `scheduleLayoutReconcile()`.
3. Push heavy storage serialization into debounce/idle callback.
4. Audit `nextTick + scheduleViewportSync` watchers; convert some to direct event-triggered updates.
5. Preserve current virtualization and pinned-column behavior.

Success criteria:
- smoother grid interactions under heavy datasets
- fewer watcher loops / timing edge cases

### P5. System Health Polling Cleanup
1. Move polling/visibility orchestration out of `AppLayout.vue` into `systemHealthStore` (or runtime service).
2. Add `refresh()` in-flight dedupe.
3. Add TTL-based `ensureFresh()` API.
4. Use WS `system_health_changed` as primary trigger; polling only as fallback (reconnect/visibility/periodic revalidate).

Success criteria:
- layout no longer owns timers for data fetching
- lower request noise and clearer lifecycle

### P6. Guardrails & Profiling (prevent regressions)
1. Dev perf counters:
- boot time
- route entry time
- fetch counts per store
- grid reconcile duration
2. Add tests for:
- in-flight dedupe
- stale response cancellation
- one-time WS listener registration
3. Define watch budget for critical files (`AllocationEditor`, `UiAffinoDataGrid`, major stores).

Success criteria:
- future regressions are visible early

## Suggested Implementation Order (PR/commit sequence)
1. P0 listener hygiene + shared viewport source
2. P5 system health orchestration move (low blast radius)
3. P1 store loading API normalization for one target page (`signals` first)
4. P2 signals page watcher consolidation
5. P3 store decomposition (channel, then switchgear)
6. P4 datagrid watcher batching and persistence cleanup
7. P6 profiling and guardrails

## Immediate Code Targets
- `frontend/src/boot/webSocket.ts`
- `frontend/src/composables/useViewport.ts`
- `frontend/src/components/layout/AppLayout.vue`
- `frontend/src/stores/systemHealthStore.ts`
- `frontend/src/pages/signals/components/AllocationEditor.vue`
- `frontend/src/components/ui/UiAffinoDataGrid.vue`
- `frontend/src/stores/channelStore.ts`
- `frontend/src/stores/switchgearStore.ts`

## Notes on Watchers (important)
Not every watcher should be removed.
Keep watchers that synchronize local DOM/UI state. Optimize watchers that:
- trigger network requests
- rebuild large data structures
- cascade into other watchers
- depend on broad tuple sources without scheduling

## Current Status (Implemented In This Pass)
Done now:
- P0.1 `bootWebSocket()` one-time reconnect hook registration (no duplicate global listeners on repeated boot calls)
- P0.2 Named WS reconnect wake handlers instead of inline lambdas
- P0.3 Shared singleton viewport source inside `useViewport()` (one shared resize listener across consumers)
- P0.4 `AppLayout.vue` switched to shared `useViewport()` instead of local resize listener
- P5.1 Base system health monitoring orchestration moved out of `AppLayout.vue` into `systemHealthStore`
- P5.2 `systemHealthStore.refresh()` in-flight dedupe added
- P5.3 `systemHealthStore.ensureFresh()` TTL-aware API added
- P5.4 `systemHealthStore.startMonitoring()/stopMonitoring()` with visibility + interval orchestration added
- P1 (partial, `/signals`): `signalSheetStore` now has TTL-aware `ensureSheetLoaded/ensureAllocationsLoaded/ensurePresetsLoaded`
- P1 (partial, `/signals`): `AllocationEditor` switched main page refresh path to `ensure*` APIs with in-flight dedupe
- P2 (partial, `/signals`): realtime scope sync watchers reduced into one consolidated trigger (includes device status changes)
- P2 (partial): `deviceStore.devicesRevision` added so page-level reactive triggers can depend on cheap revisions instead of rebuilding device signatures
- P2 (partial, `/signals`): realtime unit scope orchestration extracted from `AllocationEditor.vue` into `useSignalsRealtimeUnitScope` composable; page watcher count reduced
- P2 (partial, `/signals`): page lifecycle orchestration (workspace switch + refresh dedupe + import-query auto-open) extracted into `useSignalsPageLifecycle` composable
- P2 (partial, `/signals`): selected-row persistence switched from watcher to event-driven updates (removed one page watcher)
- P3 (started): `channelStore` command runtime state machine extracted into `frontend/src/stores/channelStore/commandRuntime.ts` (pending/debounce/error UI state, action queues, AO action map, command refresh timers)
- P3 (continued): `channelStore` command transport/actions extracted into `frontend/src/stores/channelStore/transportActions.ts` (`sendDo*`, `sendAo*`) while preserving `channelStore` external API
- P3 (continued): `channelStore` runtime reducer extracted into `frontend/src/stores/channelStore/runtimeReducer.ts` (`setChannels`, `setResponse`, state revision tracking)
- P3 (continued): `requestStates` extracted into `frontend/src/stores/channelStore/stateRequests.ts`; `channelStore.ts` now acts more like a facade/orchestrator
- P3 (continued): `channelStore` catalog/index layer extracted into `frontend/src/stores/channelStore/catalog.ts` (`fetchAll/ensureLoaded/fetchByDevice`, indexes, `setBaseChannels`, `updateChannelField`, reset path); `channelStore.ts` is now a thin facade over catalog/runtime/transport/reducer modules
- P4 (started): `UiAffinoDataGrid.vue` persistence signature watchers (`columnState/sort/groupBy`) batched into a single watcher to reduce redundant persist scheduling under linked grid mutations
- P4 (continued): coalesced repeated `nextTick(updateMeasuredHeaderHeights + scheduleViewportSync)` paths in `UiAffinoDataGrid.vue` via a small scheduler helper (reduces redundant post-tick work during column/filter bursts)
- P4 (continued): coalesced repeated `nextTick(updateObservedViewportSize + scheduleViewportSync)` paths for filter/group/rows-length mutations in `UiAffinoDataGrid.vue`
- P4 (continued): grid table-settings persistence scheduling now uses debounced `requestIdleCallback` (with `setTimeout` fallback) plus unified cancellation path to reduce serialization work during rapid UI changes
- P4 (continued): selection persistence in `UiAffinoDataGrid.vue` is now debounced + idle-scheduled (with flush-on-unmount and dataset-scope cancellation), reducing storage write bursts during bulk selection
- P4 (continued): merged `renderedColumns + overscan + rowHeight` layout/viewport watchers into a single conditional watcher in `UiAffinoDataGrid.vue` (fewer reactive callbacks for linked layout changes)
- P4 (continued): coalesced `props.columns` schema-reconcile path into one post-tick scheduler (`restorePersistedTableSettings + header measure + viewport sync`) in `UiAffinoDataGrid.vue`
- P6 (started): added lightweight dev perf tracker (`frontend/src/utils/devPerf.ts`) with counters + duration capture (DEV-only, exposed as `window.__UNITLAB_DEV_PERF__`)
- P6 (started): instrumented `bootRuntime()` and `bootWebSocket()` with dedupe counters and stage timings (`workspace bootstrap`, `store bootstrap`, websocket boot)
- P6 (continued): instrumented `runStoreBootstrap(...)` with call/dedupe/completion counters + duration timing
- P6 (continued): instrumented `signalSheetStore` `refresh*` / `ensure*` paths (cache hits, dedupe waits, refresh counts, duration timings for sheet/presets/allocations)
- P6 (continued): instrumented `signalSheetStore` hotspot mutation paths (`bulkSetAllocations`, `autoAllocate`, `ensureAllocated`, `markSignalsTested`, `applyServerAllocationPatch`) including API sub-timings and frontend patch timings
- P6 (continued): instrumented `deviceStore` (`fetchAll/ensureLoaded`) with dedupe/cache-hit counters and fetch timing
- P6 (continued): instrumented `systemHealthStore` (`refresh/ensureFresh/startMonitoring/stopMonitoring`) with dedupe/cache/monitor lifecycle counters and refresh timing
- P6 (continued): instrumented `/signals` UI-side grid refresh paths in `AllocationEditor.vue` (`rebuildGridRows`, incremental `syncGridRowsBySignalIds`, allocation-revision `rAF` scheduling/flush, `requestGridCellRefresh`) to separate store patch time from UI/grid refresh time in DEV profiling
- Final page-level watcher audit pass: `Signals/Devices/Switchgears` pages mostly retain UI-local watchers only; `SwitchgearBindingsEditor` workspace watcher switched from unconditional `refresh*` to TTL/dedupe-aware `ensure*` calls via `runStoreBootstrap`
- P3 (started, switchgear): extracted switchgear runtime resolver layer into `frontend/src/stores/switchgearStore/runtimeResolvers.ts` (bindings lookup, channel/state resolve, DO pair resolve, online status, DI-edge memory pruning helper) while keeping `switchgearStore` API stable
- P3 (continued, switchgear): extracted DI-edge switching orchestration + delay-timer engine into `frontend/src/stores/switchgearStore/diEdgeSync.ts`; `switchgearStore.ts` now mostly keeps CRUD/workspace lifecycle orchestration
- P3 (continued, switchgear): extracted CRUD/load queue layer into `frontend/src/stores/switchgearStore/catalogCrud.ts` (`fetchAll/ensureLoaded/create/update/remove`)
- P3 (continued, switchgear): extracted naming + auto-binding allocation helpers into `frontend/src/stores/switchgearStore/autoBindings.ts` (`nextDefaultName/nextDuplicateName/buildAutoBindings`)
- P3 (continued, switchgear): extracted workspace/reset/watch lifecycle orchestration into `frontend/src/stores/switchgearStore/lifecycle.ts` (workspace change reset, DI-edge sync trigger, memory pruning trigger)

Not closed yet (next implementation passes):
- P1 (loading API normalization + cancelation across stores/pages)
- P2 (`AllocationEditor.vue` watcher consolidation)
- P3 (store decomposition: switchgear split essentially complete; remaining work is optional facade polish / tests)
- P4 (`UiAffinoDataGrid.vue` watcher batching/persist/layout cleanup; started, partial)
- P6 (profiling counters + guardrail tests)
