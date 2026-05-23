# DataGrid Virtualization Support Matrix

Baseline date: `2026-05-18`

This matrix documents current virtualization support for the Vue app-stage renderer and core viewport services. It is intentionally conservative: "supported" means implemented with focused automated coverage or a repeatable performance gate; "partial" means implemented behavior exists but enterprise coverage or a key integration path is still missing.

## Public Configuration

`@affino/datagrid-vue-app` defaults to `renderMode="virtualization"`.

| Input | Row virtualization | Column virtualization | Notes |
| --- | --- | --- | --- |
| no `virtualization` prop | enabled | disabled | Compatibility default for ordinary row-heavy grids. |
| `virtualization={true}` | enabled | enabled | Use for wide grids that need horizontal windowing. |
| `virtualization={false}` | disabled | disabled | Use only for small grids or diagnostics. |
| `virtualization={{ rows, columns, rowOverscan, columnOverscan }}` | explicit | explicit | Overscan values are clamped to non-negative integers. |
| `renderMode="pagination"` | disabled | follows `columns` | Pagination owns the row window. |

## Ownership Boundary

| Area | Status | Owner | Validation |
| --- | --- | --- | --- |
| Canonical range math, clamp, overscan primitives | Supported | `@affino/datagrid-core` | Core viewport and virtualization contract suites. |
| DOM scroll sampling, rAF commits, retained render windows | Supported | `@affino/datagrid-vue` + `@affino/datagrid-vue-app` | App viewport contract tests and browser blank-viewport detectors. |
| Core/app boundary | Partial | Core owns canonical math; Vue app owns materialization | The boundary is documented, but the Vue app still has a substantial render-window path that must stay aligned with core invariants. |

## Feature Matrix

| Capability | Status | Current behavior | Remaining gap |
| --- | --- | --- | --- |
| Vertical row virtualization for client rows | Supported | Visible row windows, retained windows, adaptive overscan, stable row keys, and blank-viewport detection are covered. | Broader browser/device profiles and 1M-row browser gates. |
| Horizontal column virtualization | Supported when enabled | Core covers 1k/10k-column stress with pinned panes and mutations; Vue sandbox covers a 1000-column browser smoke path. | Browser/device coverage beyond 1000 columns and rendered resize/reorder/hide/show variants. |
| Pinned left/right columns with virtualization | Supported | Horizontal metadata and stage panes keep pinned columns synchronized with the center viewport. | Broader device and fractional-scroll browser variants. |
| Pinned bottom rows | Supported | Stage runtime renders pinned-bottom shell behavior and syncs horizontal scroll. | Additional enterprise browser coverage during complex projection changes. |
| Pinned top rows | Partial | Row state supports top pinning, but the rendered app-stage path still needs explicit verification. | Add focused component/e2e coverage or document unsupported cases. |
| Variable row heights | Partial | Vue app path supports row-height metrics and active-cell visibility with variable offsets; core supports fixed/estimated behavior plus height cache contracts. | Exact per-row variable-height virtualization is not a uniform core invariant. |
| Row resize/autosize ownership | Supported | Public Core API `view` methods own row-height mode/base/measurement/overrides. | Keep UI layers limited to gesture state. |
| `dataSourceBackedRowModel` virtualization | Supported enterprise path | Placeholder rows, viewport range sync, prefetch, stale visible-row retention, retry/failure coverage, and placeholder telemetry exist. | Broader browser/mobile latency profiles and hard-fail promotion for datasource-churn placeholder exposure. |
| `serverBackedRowModel` virtualization | Partial/simple path | Provides viewport warmup, LRU cache reuse, and compatibility adapter behavior. | Missing placeholder parity; it can underfill requested ranges while cache data is absent. Prefer `dataSourceBackedRowModel` for enterprise server-backed grids. |
| Grouped/tree rows | Supported for flattened client projections | Viewport consumes flattened row-model output; core covers grouped collapse and parent-tree collapse/re-expand near the viewport. | App-stage browser coverage, grouped/tree a11y semantics, and server/data-source grouped placeholder metadata. |
| Pivot/projected rows | Partial | Projection output can feed virtualization through the row model boundary. | Full sort/filter/group/pivot/cache-replacement lifecycle gates are still slice-based, not one combined invariant suite. |
| Active cell and keyboard navigation across remount | Supported | Focus remount and keyboard navigation beyond rendered range have browser/contract coverage. | More unloaded-row and server-placeholder interaction variants. |
| Inline edit across virtual unmount | Supported | Text edit commits when edited row or column leaves the rendered virtual window. | Additional editor mode coverage. |
| Selection and clipboard across unloaded rows | Partial | Local copy/paste/fill paths block unsafe unloaded or placeholder ranges unless server delegation exists. | Server-delegated copy/export, cut, clear/delete, paste, range move, and summary endpoints remain planned. |
| Virtualized a11y attributes | Supported at attribute level | `aria-rowcount`, `aria-colcount`, `aria-rowindex`, `aria-colindex`, selection state, and placeholder disabled metadata are covered. | Screen-reader device validation and deeper grouped/server placeholder semantics. |
| Touch/mobile scrolling | Partial | Native body scrolling, scroll-first touch policy, explicit touch handles, adaptive overscan, and lightweight custom rendering during touch scroll are implemented. | Real-device matrix and hardware threshold review are still open. |
| Custom renderers during scroll | Partial | Touch active-scroll path can bypass custom cell/group renderers and render display values. | Desktop custom-renderer frame budget and degradation policy are not isolated yet. |
| Runtime virtualization telemetry | Supported when enabled | `dgPerfTrace=1` exposes rendered counts, overscan, range resolve time, viewport update time, blank-viewport flags, placeholder rows, and stage scroll samples. | Reason/direction/velocity labels and hard mount/unmount churn budgets remain future work. |
| Enterprise browser perf gate | Supported focused gate | `bench:datagrid:enterprise:virtualization:assert` covers 100k-row vertical/server placeholder scenarios and a 10k-row / 1000-column horizontal scenario. | 1M-row and 10k-column browser gates remain broader stress work. |

## Validation Expectations

Use the smallest relevant gate first:

- Core range/math: `pnpm --filter @affino/datagrid-core exec vitest run --config vitest.config.ts src/viewport/__tests__/virtualizationRangeInvariants.contract.spec.ts src/viewport/__tests__/horizontalVirtualization.stress.contract.spec.ts`
- Vue app viewport: `pnpm --filter @affino/datagrid-vue exec vitest run --config vitest.config.ts src/app/__tests__/useDataGridAppViewport.contract.spec.ts`
- Browser blank viewport and resize paths: `pnpm exec playwright test e2e/sandbox-grid.spec.ts`
- Interaction continuity: `pnpm exec playwright test e2e/sandbox-interactions.spec.ts`
- Focused enterprise virtualization perf gate: `pnpm run bench:datagrid:enterprise:virtualization:assert`
- Docs/framework validation: `node ./scripts/check-datagrid-docs-framework-track.mjs`

## Enterprise Defaults

- Use `dataSourceBackedRowModel` for server-backed enterprise grids.
- Enable column virtualization explicitly for wide grids.
- Treat `serverBackedRowModel` as a compatibility/simple path until placeholder parity is implemented or the limitation is acceptable.
- Keep touch/mobile rollout behind real-device validation for iPad Safari/Chrome, Android Chrome, Windows touch, and macOS precision trackpad.
- Do not advertise 1M-row browser or 10k-column browser guarantees until those gates are promoted from planned stress coverage to repeatable validation.

## Not Guaranteed Yet

- 1M-row browser behavior under server-backed latency.
- 10k-column browser behavior in the rendered Vue app path.
- Placeholder parity for `serverBackedRowModel`.
- Screen-reader device behavior for grouped/tree/server-placeholder virtualized rows.
- Real-device mobile/touch performance thresholds.
- Dedicated hard budgets for row/cell mount and unmount churn.
