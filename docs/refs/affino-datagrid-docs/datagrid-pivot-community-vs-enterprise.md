# DataGrid Pivot: Community vs Enterprise

## Goal

Freeze the pivot packaging boundary before first public release.

## Community package

Package:

- `@affino/datagrid-pivot`

Keep in community:

- pivot model/type contracts
- pivot drilldown contracts
- pivot layout snapshot contracts
- pivot spec normalization / cloning / equality helpers
- base pivot semantics that are needed for broad adoption

## Enterprise package

Package:

- `@affino/datagrid-pivot-enterprise`

Reserve for enterprise:

- high-cardinality pivot runtime optimization tiers
- advanced incremental pivot patch execution
- pivot profiler / explain / diagnostics tooling
- premium pivot UX/runtime control layers
- server-backed pivot acceleration and scaling controls

## Boundary rules

1. Community package must remain useful on its own.
2. Enterprise package must be additive, not a replacement for community APIs.
3. `@affino/datagrid-pivot` must not import enterprise code.
4. Base pivot contracts and pivot model semantics stay community-safe.
5. Monetization should focus on expensive runtime/tooling/scaling layers, not basic pivot usability.
