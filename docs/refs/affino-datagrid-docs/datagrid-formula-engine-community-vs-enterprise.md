# DataGrid Formula Engine: Community vs Enterprise

## Goal

Freeze the formula-engine packaging boundary before first public release.

## Community package

Package:

- `@affino/datagrid-formula-engine`

Keep in community:

- formula parsing
- diagnostics / explain / analysis
- compile field definitions and artifacts
- formula graph and execution-plan builders
- formula value coercion / comparison helpers
- base function registry and base runtime semantics

## Enterprise package

Package:

- `@affino/datagrid-formula-engine-enterprise`

Reserve for enterprise:

- worker-owned formula execution runtime
- premium fused/vector execution tiers
- enterprise compute policies and scaling controls
- formula profiler / advanced explain tooling
- collaboration / audit / high-scale snapshot tooling around formula execution

Current enterprise boundary:

- Enterprise formula packs and runtime configuration are additive over the community parser/compiler.
- Worker-owned row-model formula execution is supported through DataGrid runtime integration, not by forking the formula language.
- Async formulas, automatic volatile scheduling, and server-backed formula evaluation are planned contract work, not shipped runtime behavior.
- Server-backed formula semantics must be defined with datasource revisions, pending/error states, and unloaded-range behavior before implementation.

## Boundary rules

1. Community package must remain useful on its own.
2. Enterprise package must be additive, not a replacement for community APIs.
3. `@affino/datagrid-formula-engine` must not import enterprise code.
4. Base formula language and compile contracts stay community-safe.
5. Monetization should focus on expensive runtime and tooling layers, not basic usability.
6. Unsupported async/volatile/server formula behavior must be documented as unsupported until the public contract is approved.
