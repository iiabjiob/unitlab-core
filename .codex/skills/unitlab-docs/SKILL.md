---
name: unitlab-docs
description: Use for UnitLab / Industrial FAT Tool documentation work, including runtime architecture notes, binary protocol docs, MQTT/WebSocket contracts, device integration guides, allocation workflows, test execution docs, audit documents, package READMEs, onboarding guides, migration notes, validation expectations, and keeping docs aligned with implemented runtime behavior.
---

# UnitLab Docs

## Scope

Use this skill whenever the task is documentation-first or when a code slice changes runtime architecture, protocol behavior, live-update semantics, device interaction, UX behavior, performance characteristics, public contracts, integration behavior, validation expectations, or migration risk.

This skill is not limited to generic frontend/backend docs. Use it for industrial runtime behavior, peripheral device flows, FAT execution semantics, and operator-facing workflows.

## Documentation Priorities

- Keep docs concise, accurate, and operationally useful.
- Prefer documentation that helps engineers safely modify runtime behavior later.
- Document runtime truth, not optimistic assumptions.
- Keep hardware, runtime, protocol, frontend, and persistence ownership explicit.
- Treat protocol contracts, runtime state transitions, and safety behavior as first-class documentation concerns.
- Prefer production-shaped examples over simulator-only shortcuts.

## Documentation Rules

- Do not document aspirational behavior as implemented behavior.
- Separate:
  - implemented behavior
  - known gaps
  - simulator behavior
  - planned work
  - operational recommendations
- Name affected packages, files, APIs, protocols, topics, events, and runtime flows when useful.
- Keep docs aligned with actual runtime behavior and protocol contracts.
- Mark unresolved runtime or safety risks explicitly.
- If docs are not needed for a code slice, final response should say:
  - `docs: not needed`

## Common Artifacts

- Architecture and audit docs under `docs/`
- Runtime lifecycle and orchestration docs
- Device integration guides
- Binary protocol specifications
- MQTT topic and payload docs
- WebSocket event contracts
- REST API integration docs
- Allocation workflow docs
- FAT execution lifecycle docs
- Retest and report-generation docs
- Package READMEs
- Performance audit and benchmark notes
- Migration notes
- Troubleshooting and operational runbooks

## Runtime Documentation Rules

- Explicitly document runtime ownership boundaries:
  - backend
  - frontend
  - runtime state
  - device state
  - protocol state
  - persisted history
- Keep lifecycle states explicit:
  - device online/offline
  - stale
  - allocated
  - armed
  - running
  - paused
  - failed
  - completed
  - aborted
- Distinguish:
  - expected state
  - observed state
  - command state
  - acknowledgement state
  - persisted evidence
- Document reconnect and recovery behavior clearly.
- Do not hide degraded-state behavior.

## Binary Protocol Documentation Rules

- Document:
  - frame version
  - frame layout
  - payload structure
  - endianess
  - scaling
  - signedness
  - units
  - checksum/validation behavior
  - command IDs
  - acknowledgement semantics
  - timeout/retry behavior
- Keep binary examples small and deterministic.
- Mark unsupported or reserved frame types explicitly.
- Keep protocol docs versioned when possible.

## MQTT / WebSocket / REST Documentation Rules

- Keep transport ownership explicit:
  - MQTT for device/backend runtime communication
  - WebSocket for live frontend runtime updates
  - REST for configuration, persistence, reports, and explicit user actions
- Document:
  - topic/event names
  - payload versions
  - ordering guarantees
  - reconnect expectations
  - retry semantics
  - stale/offline handling
- Separate command messages from state-update events in documentation.
- Avoid leaking raw protocol details into frontend integration docs unless intentional.

## Allocation and FAT Workflow Documentation

- Allocation docs should explain:
  - device compatibility rules
  - exclusive channel ownership
  - signal revision compatibility
  - retest reuse behavior
  - conflict handling
- FAT workflow docs should explain:
  - execution lifecycle
  - command flow
  - acknowledgement flow
  - mismatch handling
  - evidence persistence
  - abort/recovery behavior
- Reports should be documented as reconstruction from persisted runtime events, not transient frontend state.

## Performance Documentation Rules

- Keep benchmark assumptions explicit.
- Document:
  - batching behavior
  - invalidation rules
  - live update strategy
  - virtualization assumptions
  - cache window behavior
  - backpressure behavior
  - queue semantics
- Do not document simulator performance as hardware-proven performance.
- Mark CI baseline behavior separately from local benchmark expectations where relevant.

## Audit and Decision Records

- Separate:
  - current architecture
  - identified risks
  - recommended fixes
  - roadmap ideas
- Keep audit docs actionable.
- Prefer explicit subsystem ownership over vague architectural prose.
- Record why important runtime or protocol decisions were made when future regressions are likely.

## Migration Documentation

- Add migration notes when:
  - protocol versions change
  - MQTT topics/events change
  - WebSocket payloads change
  - runtime lifecycle changes
  - allocation semantics change
  - test execution behavior changes
  - persistence schemas change
- Document backward compatibility expectations explicitly.
- Mark destructive or incompatible changes clearly.

## Validation Documentation

- Include validation expectations when behavior is safety-sensitive, performance-sensitive, or protocol-sensitive.
- Document:
  - simulator-based validation
  - hardware validation
  - benchmark validation
  - protocol compatibility checks
  - manual operator verification steps
- Keep validation instructions reproducible.

## Doc Slice Workflow

1. Identify whether the doc is:
   - current-state documentation
   - architecture note
   - protocol specification
   - audit
   - migration guide
   - onboarding guide
   - troubleshooting guide
   - roadmap/decision record
2. Verify claims against implemented code before writing.
3. Verify runtime/protocol ownership boundaries are explicit.
4. Update status and remaining work when closing planned slices.
5. Include validation expectations where relevant.
6. Keep examples production-shaped and package-oriented.
7. Mark simulator-only assumptions clearly.

## Style

- Use direct headings and short bullets.
- Prefer explicit runtime terminology over vague abstractions.
- Prefer precise file/package/protocol references over generic statements.
- Avoid duplicating large code blocks unless they are essential API/protocol examples.
- Prefer tables or structured bullets for protocol layouts and lifecycle states.
- Mark unresolved risks explicitly.
- Keep examples deterministic and copy-safe.

## Reporting

Report only:

1. Status
2. Docs updated
3. Validation expectations added/changed
4. Remaining documentation/runtime risks, if any
5. Suggested commit message