---
name: unitlab-frontend-integration
description: Use for UnitLab Vue/DataGrid integration work involving live signal updates, device allocation UX, test execution flows, HTTP/MQTT/WebSocket adapters, selection, fill, editing, history UX, overlays, pinned panes, keyboard/touch behavior, and operator-facing industrial workflows.
---

# UnitLab Frontend Integration

## Scope

Use this skill for Vue/DataGrid integration slices that affect operator workflows, live signal interaction, device/channel allocation UX, FAT execution flows, datasource adapters, editing, selection, fill, history, overlays, pinned panes, diagnostics, and interaction behavior.

This skill is not only for generic table behavior. Use it when the frontend is acting as a live industrial runtime UI over devices, tests, telemetry, and signal-state updates.

## Frontend Priorities

- Keep the operator UI responsive during active FAT execution.
- Live updates must not break editing, selection, scrolling, overlays, or test execution flows.
- Preserve stable interaction behavior under high-frequency telemetry.
- Device/channel allocation workflows must remain predictable and reversible.
- Frontend state should reflect runtime truth, not optimistic assumptions.
- Prefer explicit interaction affordances over hidden gestures in industrial workflows.

## Working Rules

- Preserve separation between DataGrid core, Vue wrapper, app layer, sandbox/demo layer, and backend/runtime ownership.
- Prefer existing composables, runtime helpers, and local interaction patterns.
- Keep runtime/device state ownership outside presentation components.
- Avoid public prop/API changes unless proposed and approved first.
- Keep one-finger touch scroll native unless the user explicitly starts an editing, selection, resize, fill, or drag affordance.
- Do not introduce hover-only requirements for touch workflows.
- Treat pinned/header/body synchronization as a first-class UX contract.
- Treat selection, allocation, test execution, and device control as separate interaction domains.
- Avoid frontend-side duplication of backend/runtime consistency rules.

## Live Update Rules

- Live updates should apply as targeted row/cell/signal patches.
- Avoid replacing entire datasets for signal-value changes.
- Preserve selection, focus, editor state, scroll position, overlays, and active interactions during updates.
- Coalesce redundant telemetry updates where safe.
- Never coalesce away command acknowledgements, failures, alarms, mismatches, or operator-visible events.
- Keep heartbeat/online/offline updates visually separate from signal-state changes.
- Avoid re-running expensive projection/filter/group pipelines for unrelated live values.
- Do not let telemetry update frequency directly drive render frequency.

## Allocation UX Rules

- Allocation links signal-list rows to physical channels/devices.
- Allocation changes must be explicit and visually traceable.
- Conflicting allocations must be clearly surfaced before commit.
- Preserve compatibility checks: device type, DI/DO/AO capability, ranges, protocol type, channel count, and signal revision compatibility.
- Retest workflows should preserve prior allocation context when compatible.
- Allocation state changes should not unexpectedly clear selection or viewport state.

## Test Execution UX Rules

- Test lifecycle state must be visually explicit: idle, allocated, armed, running, paused, failed, completed, aborted.
- Operators must clearly distinguish:
  - expected state
  - observed controller state
  - simulator output state
  - command acknowledgement state
  - mismatch/failure state
- Live test execution must not freeze table interaction.
- Test evidence and mismatch indicators must survive live updates and scrolling.
- Reports/history views should reconstruct from persisted runtime state, not transient component state.

## Interaction Checklist

- Mouse, keyboard, touch, and coarse-pointer behavior are considered separately.
- Selection and drag gestures do not steal native scroll.
- Fill/range move/resize start only from explicit affordances on touch.
- Focus changes use `preventScroll` when preserving viewport position matters.
- Pinned panes and headers remain synchronized with the body viewport.
- Overlay and hover work is suppressed or minimized during scroll.
- Editing survives live signal updates when possible.
- Live updates do not break active text editors, dropdowns, menus, or dialogs.
- Device-control actions require explicit interaction intent.
- Dangerous actions should not rely on double-meaning gestures.

## Overlay and Diagnostics Rules

- Diagnostics overlays must not become hot-path render bottlenecks.
- Floating overlays must remain aligned during scroll and live updates.
- Overlay positioning should avoid synchronous layout reads in hot paths.
- Device status, heartbeat, latency, and mismatch indicators should update independently from heavy grid rendering.
- Debug/diagnostic tooling must be suppressible in production runtime mode.

## DataGrid Integration Rules

- Prefer targeted row/cell updates over broad invalidation.
- Preserve virtualization invariants during live updates.
- Avoid rebuilding projections unless required by the changed field.
- Keep viewport/cache ownership explicit.
- Prevent blank viewport flashes during refresh or live updates.
- Preserve keyboard navigation continuity during runtime changes.
- Avoid reactive writes inside scroll/pointer hot paths.
- Use requestAnimationFrame batching where viewport synchronization matters.
- Keep pinned columns, overlays, editors, and selections synchronized under scroll pressure.

## Frontend Runtime Boundaries

- MQTT/device protocol logic should not leak directly into view components.
- WebSocket adapters should translate runtime events into stable frontend state updates.
- REST should remain for configuration, reports, persistence, and explicit user actions.
- Frontend components should consume normalized runtime state rather than raw protocol payloads where possible.
- Keep protocol version handling centralized.

## Slice Workflow

1. Trace the interaction/event from component template → composable → runtime state → backend/runtime event.
2. Identify whether the behavior belongs in core, Vue adapter, app layer, or runtime adapter.
3. Check whether live updates can interfere with interaction state.
4. Add a narrow guard/helper when repeated interaction rules emerge.
5. Avoid introducing new synchronization ownership unless necessary.
6. Add or update focused contract tests for the interaction edge.
7. Update docs/audit notes for UX or runtime behavior changes.

## Validation

- Prefer targeted Vitest contract tests for changed composables/components.
- Run package-level type-check for affected Vue/app packages.
- For live-update changes, test selection continuity, editor persistence, overlay alignment, and scroll stability.
- For allocation flows, test conflict handling, retest reuse, and signal revision compatibility.
- For test execution flows, test lifecycle transitions, mismatch indicators, abort behavior, and runtime reconnect handling.
- For touch behavior, verify native scrolling still works during telemetry updates.
- For pinned panes, verify synchronization during scroll and live updates.
- For overlays, verify alignment during scroll, resize, virtualization, and refresh.
- Include manual browser/device verification notes for interaction-heavy changes.

## Behavioral Impact Reporting

When frontend/runtime behavior changes:

### Behavioral impact

- Describe affected interaction/runtime behavior.
- Describe affected subsystem.
- Describe risks to scrolling, editing, overlays, selection, live updates, or test execution.

### Visual verification

- [ ] Verify scroll stability during live updates.
- [ ] Verify selection continuity during telemetry changes.
- [ ] Verify editor focus preservation.
- [ ] Verify pinned/header synchronization.
- [ ] Verify overlay alignment during scroll and refresh.
- [ ] Verify touch scroll remains native.
- [ ] Verify no blank viewport appears during runtime updates.
- [ ] Verify active test execution does not freeze interactions.

## Documentation

- Update docs when runtime interaction behavior, allocation UX, test workflows, overlays, or live update semantics change.
- Keep docs grounded in implemented behavior.
- Mark simulator-only behavior clearly.
- Do not document sandbox/demo shortcuts as production runtime guarantees.

## Reporting

Report only:

1. Status
2. Behavioral impact
3. Validation run
4. Runtime/interaction risks, if any
5. Suggested commit message