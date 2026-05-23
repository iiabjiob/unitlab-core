---
name: industrial-fat-runtime-refactor
description: Use for Industrial FAT Tool backend/runtime work involving peripheral devices, live signal updates, MQTT/WebSocket flows, binary protocols, device allocation, test execution, hardware I/O state, persistence, performance, and reliability.
---

# Industrial FAT Runtime Refactor

## Scope

Use this skill for slices that touch Industrial FAT Tool runtime behavior: peripheral devices, live signal state, device/channel allocation, MQTT/WebSocket updates, binary protocol commands, test execution, hardware I/O, backend persistence, and performance-sensitive data flows.

This skill is not primarily for generic DataGrid table work. Use it when the table is only one UI surface over a live industrial testing runtime.

## Project Priorities

- Live updates from peripheral devices must stay responsive under load.
- Signal state must remain consistent across backend, frontend, devices, and test reports.
- Binary protocol paths must be explicit, compact, versioned, and testable.
- Device commands must be safe, auditable, and recoverable.
- Performance matters, but not at the cost of unsafe hardware behavior.
- Prefer deterministic runtime behavior over clever abstractions.

## Working Rules

- Preserve existing protocol contracts unless the user explicitly asks for a protocol change.
- If a public MQTT topic, WebSocket event, REST endpoint, binary frame, or device command changes, propose the shape first and wait for approval.
- Keep runtime, persistence, frontend table state, and hardware-device ownership separate.
- Avoid broad rewrites of device/session/test orchestration unless the slice requires it.
- Prefer small, traceable changes with clear failure behavior.
- Treat device IDs, signal IDs, channel IDs, allocation IDs, test run IDs, command IDs, and protocol versions as consistency boundaries.
- Do not hide hardware uncertainty behind optimistic UI success.

## Runtime Slice Workflow

1. Identify the runtime owner: device manager, protocol layer, allocation service, test runner, live update bus, persistence layer, or frontend adapter.
2. Trace the full path: frontend action → backend command → protocol message → device response → live update → persisted state → UI/report.
3. Check consistency semantics: command ordering, acknowledgement, timeout, retry, deduplication, rollback, stale device state, and partial failure.
4. Check performance risks: hot update paths, batching, backpressure, serialization cost, table refresh cost, and unnecessary reactive writes.
5. Make the smallest coherent change.
6. Add or update focused tests at the protocol/service boundary.
7. Update docs when behavior, protocol, safety rules, or deployment expectations change.

## Device and Peripheral Rules

- Treat peripheral devices as unreliable network participants.
- Every command should have a clear command ID, target device, target channel/range, timestamp, timeout, and expected acknowledgement where relevant.
- Never assume a command succeeded until the runtime has a device acknowledgement or an explicit safe fallback.
- Preserve last-known state separately from confirmed current state.
- Make offline, degraded, stale, and unknown states explicit.
- Avoid silently dropping device updates.
- Prefer idempotent commands where possible.
- Protect against duplicate command execution after reconnect or retry.
- Keep hardware safety rules close to the command execution path.

## Live Update Rules

- Live updates should be event-driven, not full-table refresh driven.
- Prefer cell/row/channel-scoped updates over replacing large datasets.
- Batch high-frequency updates before they hit the frontend.
- Apply backpressure when devices publish faster than the UI can render.
- Preserve ordering per device and per signal where required.
- Coalesce redundant state changes, but never coalesce away meaningful test events.
- Keep heartbeat, online/offline, signal state, command acknowledgement, and test event streams logically separate.
- Do not let UI virtualization or table scrolling become dependent on live update frequency.

## Binary Protocol Rules

- Keep binary frames versioned.
- Define frame type, payload length, command ID, target, flags, and checksum/validation explicitly.
- Keep endianess, scaling, signedness, and units documented.
- Avoid ambiguous payload layouts.
- Validate payload length and bounds before execution.
- Reject unknown frame versions or unsupported command types safely.
- Keep binary encode/decode logic covered by focused tests.
- Prefer compact frames for hot paths, but keep debug tooling available to inspect decoded messages.

## MQTT / WebSocket / REST Boundaries

- MQTT is for device/backend communication and backend-side runtime events.
- WebSocket is for frontend live updates and user-visible runtime state.
- REST is for configuration, persistence, reports, and explicit user actions where request/response semantics are useful.
- Do not leak raw device protocol details directly into frontend contracts unless deliberately exposed.
- Keep topic names, event names, and payload versions stable.
- Separate command requests from state updates and acknowledgements.
- Preserve reconnect behavior and resubscription rules.

## Allocation Rules

- Allocation links signal-list items to physical device channels.
- Allocation state must be stable, auditable, and reversible.
- Never allow two active signals to own the same exclusive output channel unless explicitly supported.
- Validate device capability before allocation: DI, DO, AO, protocol type, range, voltage/current mode, channel count, and safety constraints.
- Preserve allocation history for reports and retests.
- Retest should reuse prior allocation when still compatible.
- If signal-list revision changes, detect incompatible allocation mappings explicitly.

## Test Execution Rules

- Test runs must have explicit lifecycle states: created, allocated, armed, running, paused, completed, failed, aborted.
- Test steps must be ordered and replayable.
- Separate expected signal behavior from observed device/controller behavior.
- Store command events, acknowledgements, observations, mismatches, and operator actions.
- Failed or timed-out commands must produce visible test evidence.
- Avoid making reports depend on transient frontend state.
- Reports should be reconstructable from persisted test events.

## Performance Discipline

- Keep hot paths allocation-free where practical.
- Avoid full signal-list recomputation on every device update.
- Avoid full DataGrid refresh for live signal changes.
- Use keyed updates by signal ID, channel ID, or row ID.
- Batch WebSocket messages under high-frequency update load.
- Avoid JSON for high-frequency device command paths when binary protocol is available.
- Keep serialization/deserialization costs measurable.
- Add counters or diagnostics for update rate, dropped events, queue depth, command latency, ack latency, and render pressure.
- Treat scroll, editing, and selection in the table as latency-sensitive and independent from live telemetry volume.

## Persistence Discipline

- Separate runtime event logs from current-state snapshots.
- Persist enough event data to reconstruct test runs and diagnose failures.
- Keep command history append-safe.
- Avoid destructive mutation logs for test evidence.
- Use migrations for schema changes and keep them backward-compatible where possible.
- Do not store frontend-only transient state as authoritative runtime state.

## Failure Semantics

- Explicitly define behavior for offline device, stale heartbeat, command timeout, duplicate ack, out-of-order update, invalid payload, incompatible allocation, and test abort.
- Prefer safe failure over silent success.
- Surface degraded state clearly to frontend and reports.
- Partial failures must return structured results.
- Runtime should be able to recover after backend restart, device reconnect, or WebSocket reconnect without corrupting test state.

## Validation

- Prefer focused protocol/service tests first.
- For binary protocol changes, test encode/decode, invalid payloads, version mismatch, bounds, and command IDs.
- For device command changes, test ack, timeout, retry, duplicate delivery, and offline behavior.
- For live updates, test batching, ordering, coalescing, reconnect, and stale state.
- For allocation changes, test conflicts, incompatible devices, signal-list revision changes, and retest reuse.
- For test execution changes, test lifecycle transitions, failed steps, abort behavior, persisted events, and report reconstruction.
- Run package-level type-check/build for affected backend/frontend packages.
- If hardware is not available, document simulator-based validation clearly.

## Documentation

- Update protocol docs when binary frames, MQTT topics, WebSocket events, REST endpoints, or payload versions change.
- Update runtime docs when lifecycle, allocation, safety, failure, or retry behavior changes.
- Keep docs grounded in implemented behavior.
- Mark planned behavior clearly if it is not implemented yet.
- Do not document simulator shortcuts as hardware guarantees.

## Reporting

Report only:

1. Status
2. Behavior changed
3. Validation run
4. Runtime/safety risks, if any
5. Suggested commit message