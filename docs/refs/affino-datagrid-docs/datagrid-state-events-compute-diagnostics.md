# DataGrid State, Events, Compute, Diagnostics

Updated: `2026-03-03`

This document defines the stable platform-level integration surface exposed by `DataGridApi`.

## Unified state (`api.state`)

### Export

```ts
const state = api.state.get()
```

### Import

```ts
api.state.set(state, {
  applyColumns: true,
  applySelection: true,
  applyViewport: true,
  strict: false,
})
```

```ts
const migrated = api.state.migrate(externalState, { strict: true })
if (migrated) {
  api.state.set(migrated)
}
```

`api.state` is the canonical boundary for:

- workspace save/restore
- deterministic test checkpoints
- cross-runtime handoff

### V1 transport shape (current stable wire format)

- `rows.snapshot` (row-model snapshot; includes sort/filter/group/pivot/pagination/viewport/revision fields)
- `rows.aggregationModel`
- `columns` (order/visibility/widths/pins)
- `selection`
- `transaction`

Important:

- V1 state is model-centric by design (snapshot transport), not a conceptual UI-domain schema.
- `policy` and `compute mode` are not part of V1 state payload.
- `transaction` restore is intentionally unsupported in `set(...)` right now.

### Snapshot isolation contract

- Public read operations are revision-consistent inside one synchronous call stack.
- If reads are performed without intervening mutations, they observe one logical revision.
- Revision changes occur only at mutation/recompute boundaries.

## Typed public events (`api.events`)

Subscribe via `api.events.on(event, listener)`.
For cross-layer mapping between `api.events`, Vue component emits, plugins, and local feature events, see [Event matrix](./datagrid-event-matrix.md).

Stable event set:

- `rows:changed`
- `columns:changed`
- `projection:recomputed`
- `selection:changed`
- `row-selection:changed`
- `pivot:changed`
- `transaction:changed`
- `viewport:changed`
- `state:import:begin`
- `state:import:end`
- `state:imported`
- `error`

### Ordering guarantees (actual runtime contract)

- For row-model subscription ticks:
  - `rows:changed` is emitted first.
  - `projection:recomputed` is emitted after `rows:changed` if projection recompute version changed.
  - `pivot:changed` is emitted after that if pivot model/columns signature changed.
  - `viewport:changed` is emitted after that if viewport range changed.
- `columns:changed` is emitted from column-model subscription ticks.
- `selection:changed` is emitted by selection facade methods (`setSnapshot/clear`) and during `state.set(...)` when selection is applied.
- `transaction:changed` is emitted by transaction facade methods (`begin/commit/rollback/apply/undo/redo`).
- `api.state.set(...)` emits explicit restore boundaries:
  1. `state:import:begin`
  2. row/column/selection events while restore is applied
  3. `state:imported` on successful apply
  4. `state:import:end`
- reentrant event emissions are queued FIFO inside one runtime tick.

### Non-guarantees

- `api.state.set(...)` is a logical begin/end boundary, not a single-event atomic payload.
- row/column events may be emitted inside that boundary while restore is being applied.
- No cross-service total-order guarantee is declared beyond the sequencing above.

### Error handling philosophy

- Recoverable runtime conflicts are emitted through `error` events.
- Guarded operations also throw/reject to keep control-flow explicit.
- Aborted guarded mutations are represented by `aborted` code and `AbortError`.

## Compute control (`api.compute`)

```ts
if (api.compute.hasSupport()) {
  const switched = api.compute.switchMode("worker")
  const mode = api.compute.getMode()
  const diagnostics = api.compute.getDiagnostics()
}
```

Switch semantics:

- `switchMode(...)` is synchronous and returns `boolean` (`true` if mode changed).
- It does not implicitly run refresh/reapply.
- It does not alter row-model revision by itself.
- It does not coordinate active transaction batches automatically; caller should switch mode at safe lifecycle points.
- Use explicit recompute (`api.view.reapply()` / refresh path) if mode switch must be followed by projection update.

Abort semantics:

- `api.rows.patch(...)` and `api.rows.applyEdits(...)` accept `options.signal`.
- `api.transaction.apply(...)` accepts `options.signal`.
- Aborted operations fail with `AbortError`.

## Lifecycle concurrency model

- `runExclusive(...)` serializes guarded high-impact operations.
- `whenIdle()` resolves after guarded queue drain.
- `isBusy()` reflects active or queued guarded work.

Non-guarantee:

- Non-guarded row/query mutations are not automatically wrapped by lifecycle exclusivity.

## Aggregated diagnostics (`api.diagnostics`)

```ts
const diagnostics = api.diagnostics.getAll()
const formulaExplain = api.diagnostics.getFormulaExplain()
```

Domains:

- `rowModel`
- `compute`
- `derivedCache`
- `backpressure`

### Aggregated diagnostics field guide

`api.diagnostics.getAll()` returns a unified runtime snapshot. The payload is intended for inspector panels, local profiling, debugging, and support diagnostics.

#### `rowModel`

- `kind`: active row-model kind (`client`, `dataSource`, `server`, ...).
- `revision`: current snapshot revision if the model exposes one.
- `rowCount`: number of rows currently visible to the row model snapshot.
- `loading`: whether the row model is currently loading.
- `warming`: whether the model is in a warm-up phase.
- `treeData`: tree diagnostics payload when tree mode is enabled.
- `projection`: projection-pipeline diagnostics for client row models.

#### `rowModel.projection`

- `version`: projection cycle version.
- `cycleVersion`: same logical cycle counter, kept for explicit diagnostics naming.
- `recomputeVersion`: increments only when a cycle performed an actual recompute.
- `staleStages`: stages currently marked stale.
- `lastInvalidationReasons`: latest invalidation causes (`sortChanged`, `filterChanged`, ...).
- `lastInvalidatedStages`: downstream stage set implied by the latest invalidation reasons.
- `lastRecomputeHadActual`: whether the last cycle actually recomputed anything.
- `lastRecomputedStages`: stages recomputed in the last cycle.
- `lastBlockedStages`: stages intentionally blocked from recompute in the last cycle.

`rowModel.projection.performance` is intended for cost attribution:

- `totalTime`: total measured stage execution time for the last projection cycle, in milliseconds.
- `stageTimes`: per-stage execution time in milliseconds for the last projection cycle.

`rowModel.projection.pipeline` is intended for reduction-funnel analysis:

- `rowCounts.source`: source row count before projection stages.
- `rowCounts.afterCompute`: row count after compute-stage materialization.
- `rowCounts.afterFilter`: row count after filter stage.
- `rowCounts.afterSort`: row count after sort stage.
- `rowCounts.afterGroup`: row count after group stage.
- `rowCounts.afterPivot`: row count after pivot stage.
- `rowCounts.afterAggregate`: row count after aggregate stage.
- `rowCounts.afterPaginate`: row count after pagination stage.
- `rowCounts.visible`: final visible/materialized row count.

`rowModel.projection.memory` is intended as a lightweight runtime footprint estimate, not a JS heap profiler:

- `rowIndexBytes`: estimated bytes for the source row-id index.
- `sortBufferBytes`: estimated bytes used by the sort projection buffer.
- `groupBuckets`: number of grouped buckets/group rows currently materialized.
- `pivotCells`: estimated pivot cell count (`pivot rows x pivot columns`).

`rowModel.projection.formula` and `rowModel.projection.computeStage` are formula/computed-field specific diagnostics:

- `formula`: runtime formula errors, recomputed field names, compile-cache metrics.
- `computeStage`: rows touched, changed rows, field evaluations, dirty-node details.

#### `compute`

Compute diagnostics describe compute transport/runtime health:

- configured mode vs effective mode
- transport kind
- dispatch/fallback counters
- worker/sync execution visibility

Use this to answer whether formula/computed execution is actually running in sync mode, worker mode, or a fallback path.

#### `derivedCache`

Derived-cache diagnostics describe reusable runtime caches:

- filter predicate cache hits/misses
- sort value cache hits/misses
- group value cache hits/misses
- source-column cache size / limit / evictions
- cache revision tracking by row/sort/filter/group

Use this to understand whether projection is reusing cached derived values effectively or rebuilding them too often.

#### `backpressure`

Backpressure diagnostics are exposed by datasource-backed/server-backed row models when supported:

- `pullRequested`: pull requests scheduled.
- `pullCompleted`: pulls completed successfully.
- `pullAborted`: pulls aborted because they became stale.
- `pullDropped`: pulls dropped without execution.
- `pullCoalesced`: pulls merged into a newer or broader request.
- `pullDeferred`: pulls postponed until pressure decreases.
- `rowCacheEvicted`: rows evicted from remote row cache.
- `pushApplied`: push updates applied from the datasource.
- `invalidatedRows`: rows invalidated and marked stale.
- `inFlight`: whether a pull is currently running.
- `paused`: whether backpressure is manually paused.
- `hasPendingPull`: whether a deferred pull is waiting.
- `rowCacheSize`: current remote row-cache size.
- `rowCacheLimit`: configured remote row-cache limit.

Use this to understand overload behavior in server/data-source models: aborting stale pulls, coalescing viewport churn, and cache pressure.

### Annotated full snapshot example

This is a representative `api.diagnostics.getAll()` payload rendered as `jsonc` so each block can be documented inline. Real snapshots may omit blocks that are not supported by the active row model or runtime mode.

```jsonc
{
  // High-level row-model state. Present for every grid runtime.
  "rowModel": {
    "kind": "client",
    "revision": 42,
    "rowCount": 128,
    "loading": false,
    "warming": false,
    "projection": {
      // Monotonic counters for the client-side projection pipeline.
      "version": 42,
      "cycleVersion": 42,
      "recomputeVersion": 17,

      // Which stages are currently dirty and why they became dirty.
      "staleStages": [],
      "lastInvalidationReasons": ["filterChanged", "sortChanged"],
      "lastInvalidatedStages": ["filter", "sort", "paginate"],

      // What actually happened in the last projection cycle.
      "lastRecomputeHadActual": true,
      "lastRecomputedStages": ["filter", "sort", "paginate"],
      "lastBlockedStages": [],

      // Cost attribution for the last completed cycle.
      "performance": {
        "totalTime": 1.57,
        "stageTimes": {
          "compute": 0.11,
          "filter": 0.38,
          "sort": 0.74,
          "group": 0,
          "pivot": 0,
          "aggregate": 0,
          "paginate": 0.34
        }
      },

      // Row-count funnel through the projection stages.
      "pipeline": {
        "rowCounts": {
          "source": 5000,
          "afterCompute": 5000,
          "afterFilter": 481,
          "afterSort": 481,
          "afterGroup": 481,
          "afterPivot": 481,
          "afterAggregate": 481,
          "afterPaginate": 128,
          "visible": 128
        }
      },

      // Lightweight footprint estimates for projection-owned structures.
      "memory": {
        "rowIndexBytes": 40000,
        "sortBufferBytes": 3848,
        "groupBuckets": 0,
        "pivotCells": 0
      },

      // Formula runtime health inside the projection pipeline.
      "formula": {
        "errorCount": 0,
        "recomputedFields": ["grossMargin", "displayStatus"],
        "compileCache": {
          "hits": 24,
          "misses": 2,
          "size": 9
        }
      },

      // Compute-stage activity for the last cycle.
      "computeStage": {
        "rowsTouched": 481,
        "changedRows": 37,
        "fieldsTouched": ["grossMargin", "displayStatus"],
        "evaluations": 962,
        "skippedByObjectIs": 444,
        "dirtyNodes": ["grossMargin", "displayStatus"]
      }
    }
  },

  // Compute transport/runtime health.
  "compute": {
    "configuredMode": "worker",
    "effectiveMode": "worker",
    "transport": "worker",
    "dispatches": 17,
    "fallbacks": 0,
    "lastFallbackReason": null,
    "worker": {
      "inFlight": false,
      "lastRoundTripMs": 0.93
    }
  },

  // Shared reusable caches that projection depends on.
  "derivedCache": {
    "filter": {
      "hits": 930,
      "misses": 51
    },
    "sort": {
      "hits": 1440,
      "misses": 96
    },
    "group": {
      "hits": 0,
      "misses": 0
    },
    "sourceColumns": {
      "size": 12,
      "limit": 64,
      "evictions": 0
    },
    "revisions": {
      "rows": 42,
      "filter": 7,
      "sort": 11,
      "group": 0
    }
  },

  // Backpressure exists mainly on datasource/server models.
  // A client model may omit this block entirely.
  "backpressure": {
    "pullRequested": 28,
    "pullCompleted": 24,
    "pullAborted": 3,
    "pullDropped": 0,
    "pullCoalesced": 6,
    "pullDeferred": 2,
    "rowCacheEvicted": 128,
    "pushApplied": 4,
    "invalidatedRows": 512,
    "inFlight": false,
    "paused": false,
    "hasPendingPull": false,
    "rowCacheSize": 800,
    "rowCacheLimit": 1000
  }
}
```

How to read the example:

- `rowModel`: the current runtime snapshot of the active row model. Start here first to see whether the grid is client-side, server-side, still loading, or already stable.
- `rowModel.projection`: the client projection pipeline state. This is the main block for understanding why visible rows changed after sort, filter, compute, grouping, pivoting, or pagination.
- `rowModel.projection.performance`: cost attribution for the last recompute. If `totalTime` is high, `stageTimes` tells you which stage actually consumed the time.
- `rowModel.projection.pipeline`: reduction funnel. This is the fastest way to answer “where did my rows disappear?” or “which stage shrank the dataset the most?”.
- `rowModel.projection.memory`: footprint estimates for projection-owned structures. Treat these as directional counters for support/profiling, not exact heap usage.
- `rowModel.projection.formula`: formula runtime health. Check this when computed fields look stale, error-prone, or unexpectedly expensive to compile.
- `rowModel.projection.computeStage`: compute execution summary for the last cycle. Useful when the formula layer is correct but the amount of recomputation still looks too high.
- `compute`: transport/runtime mode for compute execution. This answers whether the grid is actually using worker mode, sync mode, or a fallback path.
- `derivedCache`: cache reuse health. High misses here usually explain why projection work is more expensive than expected even if the visible result is correct.
- `backpressure`: datasource/server load-management state. Read this block when scroll churn, remote fetching, or cache eviction behavior looks unstable under pressure.

What this example does not show:

- A single inline cell edit is not currently exposed as a dedicated diagnostics field like `changedCell` or `changedCells`.
- In aggregated diagnostics, the closest public signals are usually row/field-level: `lastInvalidationReasons`, `computeStage.changedRows`, `computeStage.fieldsTouched`, and downstream projection stage timings.
- If you need the exact edited cell (`rowId` + `columnKey`), use the edit/transaction layer rather than diagnostics: `DataGridEditModel.getSnapshot().edits`, `DataGridEditModel.getEdit(rowId, columnKey)`, or transaction metadata such as `meta.affectedRange`.
- This is intentional: `api.diagnostics.getAll()` is a bounded runtime-health snapshot, not a per-cell audit/event stream.

Formula explain payload includes:

- `executionPlan` (compiled formula/computed DAG snapshot)
- `projectionFormula` (runtime errors + recomputed formula names)
- `computeStage`:
  - `rowsTouched`
  - `changedRows`
  - `fieldsTouched`
  - `evaluations`
  - `skippedByObjectIs`
- `formulas`:
  - per-formula explain tree
  - dependency trace
  - function context keys
  - dependents
  - runtime dirty / recomputed / touched flags
  - dirty-cause summary (`field`, `computed`, `context`, `all`)

Cost/behavior contract:

- `getAll()` is read-only and does not trigger recompute.
- Snapshot allocations are bounded to diagnostics payload shape.
- Safe for event-driven diagnostics panels; avoid unnecessary polling in tight loops.

### Backpressure and memory guarantees (current stable surface)

Guaranteed:

- Backpressure metrics are exposed through diagnostics when supported.
- Diagnostics reads are observational and non-mutating.
- Abort-first mutation semantics are available for guarded mutation entrypoints.
- Public backpressure controls are available for supported models via `api.data.pause()/resume()/flush()`.
- `api.rows.batch(...)` coalesces facade event emission into a single deterministic event-cycle.

Not yet guaranteed at facade level:

- Global hard memory ceiling guarantees for every model implementation.
- Global “never duplicate inflight pull” guarantee.

## Policy layer (`api.policy`)

Projection policy is explicit:

- `mutable`
- `immutable`
- `excel-like`

```ts
api.policy.setProjectionMode("excel-like")
```

Policy semantics:

- `mutable`: auto-reapply enabled (projection can react immediately to edit mutations).
- `immutable`: auto-reapply disabled (projection changes require explicit reapply).
- `excel-like`: currently same runtime behavior as `immutable` for patch flows (freeze until explicit reapply).

## Integration pattern

```ts
const off = api.events.on("rows:changed", () => {
  diagnosticsPanel.set(api.diagnostics.getAll())
  checkpointStore.set(api.state.get())
})
```

This event-driven loop is preferred over hot-path polling.

## Plugins and this surface

`api.plugins` is event-driven:

- plugins can receive stable public events through `onEvent(event, payload)`.
- plugin lifecycle hooks: `onRegister`, `onDispose`.
- plugin handler failures are isolated from core event delivery.
- plugins currently do not participate in unified state serialization/deserialization.
