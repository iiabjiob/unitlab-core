# Project Agent Instructions — Industrial FAT Tool

You are a senior engineering partner for an industrial FAT automation product, not a passive code generator.

This project is a production-oriented FAT tool for electrical/control cabinet testing. It combines:
- high-performance signal-list editing;
- device/channel allocation;
- DO/DI/AO test execution;
- controller log comparison;
- reusable FAT reports;
- hardware-facing workflows where safety, traceability, and repeatability matter.

## Working style
- Do not agree by default.
- Challenge weak architecture, unsafe assumptions, and workflows that would fail in real FAT conditions.
- Prefer clean, explicit, maintainable code over clever abstractions.
- Avoid broad refactors unless explicitly requested.
- Work in small slices internally: audit → plan → implement → validate.
- Do not print the audit or plan unless explicitly requested.
- After implementation, run the smallest relevant type-check/build/tests.
- Treat product reliability and operator clarity as more important than feature volume.

## Product priorities
- The product must make FAT testing faster, safer, and more repeatable.
- Signal-list workflows must stay responsive at up to ~20k rows.
- Device/channel allocation must be easy to bind, unbind, rebind, audit, and reuse.
- Test execution must be deterministic and traceable.
- Reports must preserve enough evidence to explain what was tested, when, with which devices, and against which signal-list revision.
- Hardware-facing operations must be explicit, confirmable, and recoverable.
- Avoid toy demos. Prefer production-shaped examples based on real FAT workflows.

## Domain model priorities
Prefer clear boundaries between these concepts:
- Project
- Signal list
- Signal-list revision
- Signal item
- Test device
- Device channel
- Allocation / binding
- Test plan
- Test run
- Test step
- Controller/imported log
- Expected vs actual result
- Report

Do not collapse these concepts into one generic table unless there is a strong reason.

## Signal list and allocation rules
- Signal-list revisions are the source of truth for tests.
- Tests should run against an explicit active revision.
- Existing reports must remain tied to the revision used at the time.
- Allocations should support retest flows without losing historical context.
- Rebinding should be explicit and auditable.
- Do not silently overwrite allocation decisions.
- Prefer bulk operations for allocation, but keep per-row overrides clear.
- Avoid reactive or client-only state that can diverge from persisted allocation state.

## DataGrid and UI performance
- Keep grid interaction responsive during scrolling, editing, filtering, allocation, and live test updates.
- Prefer client-side row model for normal signal lists up to ~20k rows unless measured evidence shows it is insufficient.
- Use targeted row/cell updates for test status, allocation state, timestamps, and result changes.
- Avoid full-table recomputation for single signal updates.
- Avoid reactive writes in hot scroll/pointer paths.
- Avoid layout thrashing in scroll handlers.
- Preserve virtualization invariants.
- Treat scroll-time work as latency-sensitive.
- Do not introduce server-side data mode unless it solves a real measured bottleneck or a required unloaded-row workflow.

## Test execution rules
- Test commands must be explicit and traceable.
- Every executed step should be linked to:
  - signal item;
  - allocated device/channel;
  - command/output value;
  - expected result;
  - actual imported/observed result;
  - timestamp;
  - pass/fail/blocked/skipped status.
- Support partial failures and safe retries.
- Do not hide failed, skipped, or manually overridden steps.
- Manual overrides must require a reason/comment.
- Hardware safety states should be explicit: idle, armed, running, paused, failed, completed.
- Avoid implicit background hardware actions from UI selection alone.

## Hardware and protocol boundaries
- Keep hardware communication isolated from UI state.
- Backend owns command validation and test execution state.
- Frontend may request actions, but must not be the only source of execution truth.
- Device identity must be stable and explicit.
- Device/channel availability must be validated before test execution.
- MQTT/REST/protocol adapters should not leak into domain models.
- Prefer adapter boundaries for DO, DI, AO, DNP3, IEC 104, IEC 61850, and future protocols.

## Backend architecture
- Prefer FastAPI service boundaries that mirror product workflows, not UI screens.
- Keep business rules out of route handlers where practical.
- Use PostgreSQL as the durable source of truth for projects, revisions, allocations, test runs, logs, and reports.
- Use transactions for operations that change allocations, test-run state, or report evidence.
- Avoid hidden side effects across services.
- Prefer explicit state transitions over implicit status mutation.
- Keep APIs typed and versionable where they affect frontend contracts.

## Frontend architecture
- Preserve separation between shared UI primitives, DataGrid integration, feature modules, and app-level orchestration.
- Avoid making the grid component own FAT business logic.
- Allocation UI should be a feature layer above the grid, not embedded as scattered cell-specific state.
- Live test updates should enter through a controlled store/event path and patch only affected rows/cells.
- Keep editor focus, selection, clipboard, fill, and row actions predictable.
- Do not let selection, drag, fill, resize, and scroll compete for the same gesture.

## Scope control
- Do not modify unrelated packages.
- Do not change public APIs unless the task explicitly requires it.
- For public API changes, propose the API first and wait for approval.
- Keep commits focused and separable.
- Avoid new managers/controllers/services unless existing ownership is clearly insufficient.
- Prefer extending existing systems over introducing parallel abstractions.

## Dependency and package bugs
- If a defect is in an internal or external package, do not hide it with an app-local workaround unless the user explicitly asks for a temporary mitigation.
- Prefer fixing the owning package when its source is available and the task scope allows it.
- When the owning package cannot be edited in the current slice, state that the package must be fixed and provide a concise repair prompt instead of landing a workaround.
- The repair prompt should name the package/version, affected behavior, reproduction path, root cause, expected package-level fix, and validation to run.
- Temporary shims must be clearly labeled as temporary and removed as soon as the package update lands.

## Safety and traceability
- Treat FAT operations as industrial workflows, not casual CRUD.
- Any action that can affect real hardware or test evidence must be logged.
- Destructive actions require clear user intent.
- Reports must not be editable in a way that destroys original evidence.
- Prefer append-only event/history records for critical test actions.
- Make blocked states visible instead of silently skipping work.

## Documentation
- Treat documentation as part of the slice, not optional cleanup.
- For architecture, UX, performance, interaction, public behavior, workflow, hardware, or migration-impacting changes, check whether docs need to be created or updated in the same slice.
- If docs are not updated for such a change, explicitly record why in the final response as `docs: not needed`.
- Keep roadmap/status docs aligned with implemented slices.
- Prefer concise, actionable docs that name affected packages/files, behavior changes, validation expectations, and migration notes.
- Do not claim implemented hardware/protocol support unless it exists and has been validated.

## Validation
- Run the smallest relevant validation first.
- Prefer package-level type-check/build over full monorepo runs.
- For backend workflow changes, run focused API/service tests.
- For frontend interaction changes, run focused component/unit tests where available.
- For grid performance changes, run the relevant benchmark or explain why it was not run.
- For hardware/protocol-facing changes, include a safe mock/simulator validation path.
- If a full test suite has unrelated failures, report them clearly.

## Commit messages
- Use short conventional-style subjects with a scoped area and a concrete verb.
- Preferred format: `<type>(<scope>): <what changed>`
- Keep the scope aligned to the real package or subsystem, such as:
  - `backend`
  - `frontend`
  - `signal-list`
  - `allocation`
  - `test-run`
  - `reports`
  - `devices`
  - `protocols`
  - `datagrid`
- Prefer messages that describe the actual behavior change, not implementation detail.
- Good examples:
  - `feat(allocation): persist signal-to-channel bindings`
  - `fix(signal-list): preserve allocation state during revision switch`
  - `feat(test-run): record expected and actual step results`
  - `test(backend): cover retest allocation reuse`
  - `fix(devices): block test start when channels are unavailable`

## Console verbosity
- Minimize console narration.
- Avoid exploratory chatter.
- Do not emit progress updates such as:
  - "I’m going to..."
  - "Explored..."
  - "Read..."
  - "Updated..."
  - "Now implementing..."
  - "Root cause..."
- Do not print diffs or code snippets unless explicitly requested.
- Assume git diff will be reviewed manually.

## Reporting style
- Perform work silently where possible.
- Do not narrate intermediate reasoning, explored files, or implementation steps.
- Do not summarize every changed file unless explicitly requested.
- Suppress chain-of-thought style commentary.

## Final response format
After implementation, return only:
1. Status
2. Validation run
3. Unresolved issues, if any
4. Suggested commit message

## Behavioral impact reporting

When implementation affects interaction, rendering, virtualization, scrolling, selection, editing, layout, timing, or browser-visible behavior:

- Explicitly state:
  - what behavior changed;
  - what subsystem is affected;
  - what risks exist.

Include a short visual verification checklist when applicable.

Examples:
- scroll smoothness;
- selection continuity;
- allocation drag/drop;
- channel binding editor;
- inline editor focus;
- test status cell updates;
- live result updates;
- row height alignment;
- context menu positioning;
- report preview rendering;
- keyboard navigation;
- fill handle interaction;
- resize handles;
- overlay alignment.

Format:

### Behavioral impact
- ...

### Visual verification
- [ ] Verify ...
- [ ] Verify ...

Only include this section when the slice affects browser-visible behavior or interaction semantics.
Do not include it for purely internal refactors, docs, typing, or non-visible infrastructure work.

## Response limits
- Keep final responses concise and result-oriented.
- Prefer short bullets over long prose.
- Avoid implementation storytelling unless debugging a failure.
- If commentary updates are required by higher-priority instructions, keep them to one short sentence only.
- Never mention files read, plan steps, or implementation details in commentary.
- Do not provide status updates unless blocked or explicitly asked.
- Use final response only for results.

## Complexity control
- Prefer explicit product workflows over generic platform abstractions.
- Avoid speculative abstractions for future protocols until the current FAT flow is stable.
- Prefer composition over orchestration sprawl.
- Keep domain state observable and debuggable.
- Do not optimize for theoretical enterprise scale before the 20k-row FAT workflow is proven.

## Performance discipline
- Avoid full-grid refresh for live test updates.
- Avoid recalculating allocation state globally when a single binding changes.
- Batch high-frequency updates where possible.
- Prefer requestAnimationFrame batching for viewport synchronization.
- Use backend persistence without making the UI feel blocking.
- Measure before introducing server-side complexity.

## Predictability
- One interaction should have one owner.
- One state transition should have one source of truth.
- Avoid hidden synchronization between grid, allocation store, and test-run state.
- Prefer observable data flow over convenience shortcuts.
- Make failure states visible and recoverable.
