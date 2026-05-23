---
name: unitlab-performance
description: Use for UnitLab / Industrial FAT Tool performance work involving live device updates, DataGrid responsiveness, virtualization, viewport/cache windows, binary protocol throughput, MQTT/WebSocket batching, render latency, invalidation, broad refresh avoidance, benchmarks, performance traces, and CI regression gates.
---

# UnitLab Performance

## Scope

Use this skill for performance-sensitive UnitLab slices: live signal updates, peripheral device telemetry, binary protocol throughput, MQTT/WebSocket event flow, DataGrid scrolling, virtualization, overscan, cache windows, render batching, invalidation, memory churn, benchmark coverage, and CI performance gates.

This skill is not only for table performance. Use it when runtime data volume, live updates, device events, or frontend rendering can affect FAT execution smoothness or operator experience.

## Performance Priorities

- Keep the operator UI responsive during live FAT execution.
- Live updates must not cause full-table refreshes.
- Device events must not block scrolling, editing, selection, or test control interactions.
- Prefer scoped updates by signal ID, row ID, device ID, channel ID, or test step ID.
- Preserve deterministic runtime behavior under load.
- Measure from concrete code paths; do not invent bottlenecks.
- Performance improvements must not weaken hardware safety, command acknowledgement, or test evidence.

## Runtime Hot Paths

- Device update ingestion.
- Binary frame encode/decode.
- MQTT publish/subscribe loops.
- WebSocket fanout to frontend clients.
- Signal-state reducer / current-state snapshot updates.
- Test runner command dispatch and acknowledgement handling.
- Device/channel allocation checks.
- DataGrid row/cell update application.
- Scroll, viewport, selection, editing, overlays, and visible-row rendering.
- Report/event-log persistence during active tests.

## Performance Rules

- Keep hot paths allocation-light where practical.
- Avoid full signal-list recomputation on every device update.
- Avoid full DataGrid refresh for live signal changes.
- Prefer patch-style updates over replacing large arrays or row models.
- Batch high-frequency updates before they hit the frontend.
- Coalesce redundant state updates, but never drop meaningful test events.
- Keep scroll handlers light and batch reactive work through `requestAnimationFrame` where practical.
- Avoid layout reads after writes in the same hot path.
- Avoid layout thrashing from `getBoundingClientRect`, `clientWidth`, `scrollTop`, computed styles, or forced synchronous measurement.
- Prefer retaining cached ranges/models over broad refreshes.
- Make overscan, cache windows, and prefetch behavior explicit and testable.
- Keep user-facing smoothness and blank-viewport prevention as primary scroll goals.
- Do not let WebSocket update frequency control DataGrid render frequency directly.
- Do not let diagnostics, logging, or debug overlays become hot-path bottlenecks.

## Live Update Rules

- Separate ingestion rate from render rate.
- Use bounded queues or backpressure when devices publish faster than the UI can consume.
- Preserve ordering per device, signal, or command where correctness requires it.
- Batch WebSocket messages under high-frequency load.
- Prefer latest-state snapshots for UI rendering and append-only events for test evidence.
- Do not coalesce away command acknowledgements, mismatches, failures, or operator actions.
- Keep heartbeat/online/offline updates separate from signal-value updates when possible.
- Track queue depth, dropped/coalesced updates, update latency, ack latency, and render pressure.

## Binary Protocol Performance

- Keep encode/decode paths small, explicit, and tested.
- Avoid JSON in high-frequency device command/update paths when the binary protocol is available.
- Validate frame length and bounds without excessive copying.
- Avoid repeated buffer allocation in tight loops where reusable buffers are safe.
- Keep scaling, endianess, signedness, and units deterministic.
- Add decode benchmarks when changing frame layout or parser behavior.

## DataGrid Performance Rules

- Apply live signal changes as targeted row/cell updates.
- Avoid rebuilding sort/filter/group projections unless the changed field requires it.
- Avoid invalidating viewport/cache windows for unrelated live values.
- Keep virtualization independent from live update frequency.
- Preserve selection, editor focus, scroll position, and overlay alignment during updates.
- Avoid reactive writes in scroll/pointer hot paths.
- Prefer requestAnimationFrame batching for viewport synchronization.
- Prevent blank viewports during fast scroll, refresh, and server/cache loading.
- Keep hover/focus/overlay work suppressed or minimized during momentum scroll.

## Backend Performance Rules

- Keep current-state snapshots separate from append-only event logs.
- Avoid synchronous persistence on every high-frequency telemetry update unless required for test evidence.
- Batch persistence where safe.
- Avoid N+1 queries in allocation, test setup, report generation, and signal-list loading.
- Keep indexes aligned with hot queries: project, test run, signal ID, device ID, channel ID, timestamp, and revision.
- Measure command latency and acknowledgement latency separately.
- Avoid blocking the event loop with CPU-heavy parsing, report generation, or bulk projection work.

## Audit Checklist

- Device update rate and burst behavior.
- MQTT subscription loop and message dispatch cost.
- WebSocket batching, fanout, reconnect, and client backpressure.
- Binary frame parser allocation and validation cost.
- Current-state snapshot update cost.
- Event-log persistence frequency and transaction size.
- DataGrid row/cell update path.
- Full refresh or broad invalidation triggers.
- Scroll event frequency and reactive writes.
- Visible range calculation and overscan policy.
- Header/body/pinned synchronization.
- Server-backed cache loading and stale/blank viewport behavior.
- Forced reflow risks: `getBoundingClientRect`, `clientWidth`, `scrollTop`, style writes.
- Render churn during momentum scroll.
- Hover/focus/overlay work while scrolling.
- Diagnostics/logging overhead in hot paths.
- Memory churn and retained object growth during long FAT runs.

## Slice Workflow

1. Identify the hot path and its callers.
2. Determine whether the issue is CPU, layout, rendering, network/cache, serialization, persistence, or synchronization.
3. Check whether the performance issue can affect hardware safety, command ordering, acknowledgement, or test evidence.
4. Measure or reason from concrete code paths.
5. Make a small change that reduces work, batches updates, avoids blanking, or narrows invalidation without changing public API.
6. Add focused tests, benchmark coverage, or a perf trace expectation.
7. Update performance docs or audit status for non-trivial changes.

## Validation

- Run targeted unit/contract tests first.
- Run existing benchmark files when the changed path has one.
- For binary protocol changes, test encode/decode correctness and benchmark parser hot paths where relevant.
- For live update changes, test batching, ordering, reconnect, backpressure, and stale-state handling.
- For DataGrid changes, test scroll stability, targeted updates, selection continuity, editor focus, and viewport/cache behavior.
- For backend changes, test burst ingestion, persistence batching, query shape, and command latency where relevant.
- For CI gate changes, compare against the correct local or CI baseline profile.
- For scroll UX, include manual verification notes for desktop and touch/coarse devices when needed.
- If hardware is unavailable, document simulator-based validation clearly.

## Metrics to Report

- Update ingestion rate.
- WebSocket message rate.
- Coalesced/dropped update count.
- Queue depth.
- Command latency.
- Ack latency.
- Render latency.
- Scroll long tasks.
- Frame drops or blank viewport incidents.
- Memory growth during soak.
- Backend query latency.
- Persistence batch latency.

## Documentation

- Update performance docs when hot-path behavior, batching, invalidation, cache windows, protocol throughput, or CI gates change.
- Keep benchmark assumptions explicit.
- Mark simulator-only validation clearly.
- Do not document optimistic benchmark behavior as hardware-proven performance.

## Reporting

Report only:

1. Status
2. Performance behavior changed
3. Validation run
4. Runtime/performance risks, if any
5. Suggested commit message