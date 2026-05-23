# DataGrid Architecture and Package Boundaries

Baseline date: `2026-05-17`
Scope: `@affino/datagrid-core`, `@affino/datagrid-orchestration`, `@affino/datagrid-vue`, `@affino/datagrid-vue-app`

## Goals

- Keep core runtime framework-agnostic and deterministic under heavy scroll/select workloads.
- Keep Vue package as a thin adapter layer, not a second runtime.
- Keep public API narrow and semver-protected while internals continue to evolve.

## Package Boundaries

| Package | Owns | Must not own |
| --- | --- | --- |
| `@affino/datagrid-core` | types, settings adapter contract, runtime signals, viewport controllers, virtualization math, selection geometry/contracts | Vue refs/watchers, SFC rendering concerns, Pinia store details |
| `@affino/datagrid-orchestration` | reusable pointer, fill, range move, header resize, keyboard, context menu, viewport blur, scroll, and telemetry lifecycles | row-model mutation policy, SFC rendering, app-specific history/server fill decisions |
| `@affino/datagrid-vue` | Vue composables, app-controller wiring, adapter lifecycle (`init/sync/teardown/diagnostics`), Pinia settings bridge, app-level interaction diagnostics, adapter materialization from canonical runtime windows | canonical virtualization math, duplicate coordinate conversion logic, core business invariants |
| `@affino/datagrid-vue-app` | mounted table stage, DOM event binding, native body viewport, viewport materialization, header/pinned pane wiring, overlays, focus surfaces, editors, sandbox-shaped UX composition | core viewport range/clamp math, model mutation primitives, stable public core API ownership |

## Dependency Direction

- `datagrid-core` has no dependency on Vue.
- `datagrid-orchestration` is framework-light shared interaction infrastructure.
- `datagrid-vue` depends on `datagrid-core` and consumes core contracts.
- `datagrid-vue-app` composes `datagrid-vue` app hooks and orchestration-backed behavior into the mounted stage.
- Adapter boundary normalizes host input before runtime; core row-node and column contracts stay canonical.

## Stable Public API

- Core stable surface: `packages/datagrid-core/src/public.ts`
- Vue stable surface: `packages/datagrid-vue/src/public.ts`
- Orchestration root: advanced adapter-internal surface documented in `docs/datagrid-orchestration-public-contract.md`; it is not an app-facing stable API.
- Rule: stable app consumers import only stable package roots. Advanced adapter consumers may use documented advanced entrypoints, including the orchestration root, when they accept that tier's compatibility contract.

## Runtime Pipeline (Canonical)

1. Input events enter adapter/composables.
2. Adapter converts input into core-safe contracts.
3. Core owns canonical viewport state (virtualization range, overscan, clamp).
4. Core emits deterministic geometry for selection/overlay.
5. Vue layer materializes and renders view state, without re-owning canonical virtualization/clamp rules.

Adapter materialization note:

- Core owns canonical viewport virtualization, range math, overscan, and clamp contracts.
- Vue/app code may sample DOM scroll/size, schedule rAF commits, retain visible rows, derive render spacers, and materialize rows/columns for the mounted table stage.
- That adapter work is render-window materialization from canonical runtime/model state; it must not become a second authoritative virtualization runtime.

Terminology:

- Projection = logical dataset transformation (`filter`, `sort`, `group`, `pivot`, `tree`, `aggregation`).
- Materialization = render-oriented realization of projected/runtime state into rows, columns, overlays, spacers, and mounted DOM.
- Runtime state = canonical state owned by core services. View materialization may cache or derive from runtime state, but must not fork runtime authority.

## Interaction Ownership

The mounted stage has one owner per active gesture. `packages/datagrid-vue/src/app/dataGridInteractionOwner.ts` is the current internal diagnostic contract for active owner snapshots. It is used to keep drag selection, fill, range move, column resize, and row resize mutually exclusive in the app path.

| Area | Owner |
| --- | --- |
| Native body scroll | `@affino/datagrid-vue-app` stage viewport runtime, backed by `@affino/datagrid-vue` app viewport sync |
| Canonical viewport math, virtualization range, overscan, clamp | `@affino/datagrid-core` viewport services |
| DOM scroll sampling, rAF viewport commits, render-window materialization | `@affino/datagrid-vue` app viewport sync and `@affino/datagrid-vue-app` table stage |
| Drag selection, fill, range move app state | `@affino/datagrid-vue` app interaction controller |
| Shared interaction lifecycle helpers | `@affino/datagrid-orchestration` |
| Column resize lifecycle | `@affino/datagrid-orchestration` with `@affino/datagrid-vue` app wrapper |
| Row resize lifecycle | `@affino/datagrid-vue` app row sizing |
| DOM binding, panes, overlays, editor/focus surfaces | `@affino/datagrid-vue-app` table stage |
| Keyboard command routing | `@affino/datagrid-orchestration` command router, wired by `@affino/datagrid-vue` |
| Context menu routing | `@affino/datagrid-orchestration` router and `@affino/datagrid-vue-app` stage handlers |

Boundary rules:

- Do not make header, pinned panes, or overlays independent scroll owners; they route through the body viewport.
- Do not add a second app-level interaction manager when an orchestration utility already owns the lifecycle shape.
- Only core may own canonical runtime state. Vue/app layers may cache, project, or materialize state, but must not become a second runtime authority.
- Keep touch body-cell gestures scroll-first; touch selection, fill, range move, and resize must start from explicit touch affordances or documented mode transitions.
- Keep active-owner diagnostics internal unless a public diagnostics API is separately approved.

## Selection Ownership

Selection has one logical state machine with package-specific ownership. Core owns pure range geometry and snapshot contracts; Vue app composables own transition policy and operation eligibility; the mounted stage owns DOM focus, rendered state, overlays, handles, and editor surfaces.

| Selection area | Owner |
| --- | --- |
| Normalized cell ranges, active range index, active cell shape | `@affino/datagrid-core` selection helpers and snapshot contracts |
| App selection snapshot, anchor, virtual-selection metadata, aggregate labels | `@affino/datagrid-vue` app selection composables |
| Row-selection mode, selected/excluded row ids, focused row | `@affino/datagrid-core` row-selection helpers with `@affino/datagrid-vue` app row-selection wiring |
| Keyboard selection and navigation transitions | `@affino/datagrid-orchestration` command/navigation utilities wired by `@affino/datagrid-vue` |
| Drag selection, fill preview, range-move preview, edit handoff | `@affino/datagrid-vue` app interaction controller with orchestration lifecycle helpers |
| DOM focus restore, selected cell classes, overlays, handles, editor mount points | `@affino/datagrid-vue-app` table stage |
| Clipboard source ranges and local mutation planning | `@affino/datagrid-vue` app clipboard wiring with orchestration clipboard helpers |

Selection transition rules:

- The committed selection snapshot is the logical source of truth; rendered overlays and classes are derived materialization.
- DOM focus may follow the active cell, but it must not become the source of truth for selected ranges or active cell identity.
- Editing, fill, and range move are temporary owners that must either commit/cancel before selection changes or explicitly hand control back to selection.
- Row selection and cell-range selection are separate state machines. Shared gestures must choose one target before mutating state.
- Projection and cache changes must use an explicit invalidation policy for active cell, ranges, row selection, virtual metadata, clipboard ranges, fill preview, and range-move preview.
- Unloaded rows, placeholders, and stale projection identity are operation states, not rendering accidents; local materialized operations must block or delegate instead of guessing.

## Hard Invariants

- One owner for scroll transform synchronization.
- One active interaction owner for pointer-driven drag, fill, range move, resize, and touch pan flows.
- One committed selection snapshot as the source of truth for cell ranges and active cell identity.
- One explicit invalidation policy for projection/cache changes that affect selection-related state.
- One canonical pin contract in runtime: `pin = left | right | none`.
- One coordinate conversion contract for `world`, `viewport`, and `client` spaces.
- Horizontal virtualization clamp and update path stays pure and deterministic.

## Core Modules to Keep Stable

- Runtime: `packages/datagrid-core/src/runtime/dataGridRuntime.ts`
- Viewport: `packages/datagrid-core/src/viewport/dataGridViewportController.ts`
- Horizontal clamp: `packages/datagrid-core/src/viewport/dataGridViewportHorizontalClamp.ts`
- Coordinate conversion: `packages/datagrid-core/src/selection/coordinateSpace.ts`

## Vue Adapter Modules to Keep Thin

- Lifecycle boundary: `packages/datagrid-vue/src/adapters/adapterLifecycle.ts`
- Headless adapter: `packages/datagrid-vue/src/adapters/selectionHeadlessAdapter.ts`
- Vue bridge: `packages/datagrid-vue/src/adapters/selectionControllerAdapter.ts`
- Pin normalization: `packages/datagrid-vue/src/adapters/columnPinNormalization.ts`

## Quality and Operations References

- Pipeline and closure log: `docs/archive/datagrid/checklists/datagrid-engine-9.5-pipeline-checklist.md`
- Quality gates: `docs/datagrid-quality-gates.md`

                          ┌──────────────────────────┐
                          │        UI Layer          │
                          │  Vue / React Adapter     │
                          │                          │
                          │  DataGrid Component      │
                          │  DOM Rendering           │
                          │  Viewport Materialization│
                          │  Cell Rendering          │
                          └────────────┬─────────────┘
                                       │
                                       │
                          ┌────────────▼─────────────┐
                          │      Runtime State       │
                          │                          │
                          │  Selection               │
                          │  Editing                 │
                          │  Expansion               │
                          │  Pagination              │
                          │  Viewport                │
                          └────────────┬─────────────┘
                                       │
                                       │
                          ┌────────────▼─────────────┐
                          │      Materialization     │
                          │                          │
                          │  Computed Overlay        │
                          │  Row Assembly            │
                          │  Snapshot Restore        │
                          │  Undo / Redo             │
                          └────────────┬─────────────┘
                                       │
                                       │
                          ┌────────────▼─────────────┐
                          │      Projection Engine   │
                          │                          │
                          │  Filter                  │
                          │  Sort                    │
                          │  Group                   │
                          │  Pivot                   │
                          │  Tree                    │
                          │  Aggregation             │
                          └────────────┬─────────────┘
                                       │
                                       │
                          ┌────────────▼─────────────┐
                          │       Compute Engine     │
                          │                          │
                          │  Compute Runtime         │
                          │  Compute Modules         │
                          │                          │
                          │   • Formula Module       │
                          │   • Aggregation Module   │
                          │   • Pivot Module         │
                          │   • Custom Modules       │
                          └────────────┬─────────────┘
                                       │
                                       │
                          ┌────────────▼─────────────┐
                          │     Dependency Graph     │
                          │                          │
                          │  Field Dependencies      │
                          │  Computed Dependencies   │
                          │  Meta Dependencies       │
                          │                          │
                          │  DAG Execution Order     │
                          └────────────┬─────────────┘
                                       │
                                       │
                          ┌────────────▼─────────────┐
                          │      Formula Engine      │
                          │                          │
                          │  Parser                  │
                          │  AST                     │
                          │  Compiler                │
                          │  JIT / AST Evaluator     │
                          │  Columnar Kernels        │
                          │  Diagnostics / Explain   │
                          └────────────┬─────────────┘
                                       │
                                       │
                          ┌────────────▼─────────────┐
                          │        Row Source        │
                          │                          │
                          │  Client Rows             │
                          │  Server DataSource       │
                          │  Streaming Updates       │
                          │                          │
                          └──────────────────────────┘
