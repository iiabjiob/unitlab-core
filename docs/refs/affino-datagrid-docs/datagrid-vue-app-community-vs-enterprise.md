# DataGrid Vue App: Community vs Enterprise

## Goal

Freeze the app-layer packaging boundary before first public release.

The current repo already has three distinct layers:

- `@affino/datagrid-core` for runtime/model/api
- `@affino/datagrid-vue` for the Vue adapter and orchestration layer
- `@affino/datagrid-vue-app` for the opinionated `<DataGrid />` app facade

This document defines how the app facade should split into community and
enterprise without making the OSS package feel crippled.

## Package roles

Community package:

- `@affino/datagrid-vue-app`

Enterprise package:

- `@affino/datagrid-vue-app-enterprise`

Internal enterprise support packages:

- `@affino/datagrid-formula-engine-enterprise`
- `@affino/datagrid-diagnostics-enterprise`
- future performance / worker enterprise package if needed

Boundary rule:

- `@affino/datagrid-vue-app-enterprise` may depend on `@affino/datagrid-vue-app`
- internal enterprise support packages may depend on community packages
- community packages must never depend on enterprise packages

## Current code ownership

Current community app facade entrypoints:

- `packages/datagrid-vue-app/src/DataGrid.ts`
- `packages/datagrid-vue-app/src/DataGridDefaultRenderer.ts`
- `packages/datagrid-vue-app/src/DataGridModuleHost.ts`
- `packages/datagrid-vue-app/src/index.ts`

Current built-in app modules wired in the default renderer:

- column menu
- column layout
- advanced filter
- aggregations

Current public extension seam for additive app-toolbar customization:

- `toolbarModules` prop on `DataGrid`
- typed by `DataGridAppToolbarModule`
- appends custom toolbar buttons/popovers after built-in community modules

Current enterprise-owned app additions:

- diagnostics toolbar module via `@affino/datagrid-vue-app-enterprise`
- formula runtime controls via enterprise-owned row-model assembly
- formula packs via `@affino/datagrid-formula-engine-enterprise`
- performance / worker presets via `@affino/datagrid-vue-app-enterprise`

The module host is an internal assembly primitive, not a product/package
boundary on its own.

## Keep in community

Keep these in `@affino/datagrid-vue-app`:

- `DataGrid` as the default app-facing component
- declarative columns / state / theme / pagination / virtualization
- saved-view facade helpers around unified state + `viewMode`
- additive toolbar customization through public `toolbarModules`
- column menu, including declarative built-in section and action config (`items`, `disabled`, `disabledReasons`, `labels`, `actions`, per-column overrides) and async/server-side value-filter histograms
- column layout, including `columnLayout.labels` for toolbar and panel localization
- advanced filter, including `advancedFilter.labels` for toolbar text, panel copy, join labels, and operator labels
- aggregations
- row hover / striped rows / base table-stage UX
- base formula usage already exposed through the community stack

These are part of the adoption story and should remain usable without a
commercial package.

## Reserve for enterprise

Move or add these through `@affino/datagrid-vue-app-enterprise`:

- diagnostics panel and premium runtime tooling
- advanced formula runtime controls and premium formula packs
- worker/performance orchestration and large-dataset acceleration controls
- license-gated enterprise app presets

Pre-release migration target from current code:

- `diagnostics` app surface now lives in
  `packages/datagrid-vue-app-enterprise/src/DataGrid.ts`
- formula runtime controls now live in
  `packages/datagrid-vue-app-enterprise/src/DataGrid.ts`
- premium formula packs now live in
  `packages/datagrid-formula-engine-enterprise/src/formulaPacks.ts`
- performance presets now live in
  `packages/datagrid-vue-app-enterprise/src/dataGridPerformance.ts`

Diagnostics is the first app-level enterprise candidate because it is useful,
defensible, and does not undermine the basic usability of the community grid.

## Public package UX

Community user:

```ts
import { DataGrid } from "@affino/datagrid-vue-app"
```

Community-safe additive toolbar customization:

```ts
import {
   DataGrid,
   type DataGridAppToolbarModule,
} from "@affino/datagrid-vue-app"
```

Use `toolbarModules` when the app only needs to append custom toolbar actions.
Do not expose `DataGridModuleHost` itself as a public plugin platform.

Enterprise user:

```ts
import { DataGrid } from "@affino/datagrid-vue-app-enterprise"
```

Upgrade path:

- change one dependency
- change one import path
- keep the same mental model and mostly the same component usage
- provide a structured `licenseKey` or app-level enterprise license provider

## Implementation pipeline for this repo

1. Create `packages/datagrid-vue-app-enterprise` as an additive shell package
   that re-exports the community app facade.
2. Keep `DataGridModuleHost` internal and do not turn it into a public generic
   plugin platform yet.
   Public additive customization for community users goes through
   `DataGrid.toolbarModules`, not through direct module-host ownership.
3. [x] Move `diagnostics` out of the community public story before first release:
   keep low-level runtime diagnostics APIs community-safe, but route premium app
   diagnostics UI through `@affino/datagrid-vue-app-enterprise`.
4. [x] Add enterprise-only app surface in the enterprise package:
   `licenseKey`, premium diagnostics controls, advanced formula runtime
   controls, performance/worker presets.
   Current state: `licenseKey`, premium diagnostics, and formula runtime
   controls are wired through `@affino/datagrid-vue-app-enterprise`.
   The enterprise package now resolves structured signed keys
   (`affino-dg-v1:<tier>:<customer>:<expiresAt>:<features>:<checksum>`) and
   supports app-level license provisioning through
   `provideAffinoDataGridEnterpriseLicense(...)`.
   Premium formula packs are routed through
   `@affino/datagrid-formula-engine-enterprise` and enabled from the enterprise
   app wrapper.
   Performance/worker presets are routed through enterprise-only row-model
   assembly and virtualization defaults:
   `balanced` for mixed interactive usage,
   `throughput` for sustained worker throughput,
   `formulaHeavy` for formula-dense grids,
   `patchHeavy` for mutation-heavy workloads.
   Formula runtime no longer relies on a hidden community prop; the enterprise
   wrapper owns row-model assembly through `@affino/datagrid-vue-app/internal`.
   Blocked premium requests are visible in the enterprise toolbar, not only in
   the console, so missing/expired/scoped licenses are observable in product UI.
5. Wire `@affino/datagrid-vue-app-enterprise` to enterprise support packages
   instead of leaking enterprise code into `@affino/datagrid-vue-app`.

## Release guardrails

Before first public release:

1. `@affino/datagrid-vue-app` must remain useful on its own.
2. `@affino/datagrid-vue-app-enterprise` must be additive, not a replacement.
3. Enterprise code must never ship inside the community package tarball.
4. Community docs must not require enterprise imports.
5. Enterprise app UX should stay a strict superset of the community app UX.
