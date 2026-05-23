# Codex Reference Map

This map keeps Codex work grounded in the existing DataGrid architecture and reduces repeated discovery before focused changes.

## First Read

- `docs/README.md` - documentation entry points.
- `docs/datagrid-architecture.md` - package boundaries, dependency direction, stable public API, and runtime invariants.

## Code Review And Bug Fixes

- `docs/datagrid-troubleshooting-runbook.md` - triage order, common viewport/overlay/pinning/selection symptoms, and focused specs.
- `docs/datagrid-strict-contract-testing.md` - mandatory contract suites and strict test commands.

Use these before changing shared runtime behavior. Name the invariant being protected, keep the diff narrow, and add or update the closest contract test.

## Quality Gates

- `docs/perf/datagrid-performance-gates.md` - SLA targets, benchmark harnesses, fail-fast rules, and CI lock commands.
- `docs/perf/datagrid-perf-by-design-runtime.md` - runtime performance contract reference.
- `docs/scripts-cheatsheet.ru-en.md` - local command descriptions.

Start with the smallest relevant package-level or script-level check, then escalate to quality locks only when the slice affects shared behavior or CI gates.

## Interaction, Scroll, And Virtualization

- `docs/MOBILE_TOUCH_SCROLL_AUDIT.md` - current scroll/touch status, known gaps, and validation risks.
- `docs/datagrid-viewport-controller-decomposition.md` - viewport service boundaries.
- `docs/datagrid-viewport-math-engine.md` - pure viewport math and scroll IO boundary.
- `docs/datagrid-viewport-rowmodel-boundary.md` - row-model boundary for viewport consumption.

Before making these changes:
- preserve existing desktop behavior
- avoid duplicate scroll, transform, coordinate, or virtualization ownership
- prefer minimal focused diffs
- add or update focused tests
- run focused checks

## Server Datasource

- `docs/server-datasource/integration-docs-map.md` - ordered reading path for package users and Codex agents.
- `docs/server-datasource/protocol.md` - HTTP datasource contract.
- `docs/server-datasource/ux-contract.md` - sandbox-equivalent UX behavior.
- `docs/server-datasource/consistency.md` - revision, dataset version, invalidation, and conflict model.
- `docs/server-datasource/checklist.md` - integration verification list.

Do not replace datasource state with app-level reload workarounds. Keep row-model, adapter, backend protocol, history, and consistency responsibilities explicit.
