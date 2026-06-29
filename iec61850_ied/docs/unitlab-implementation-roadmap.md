# UnitLab Implementation Roadmap

Status: working execution roadmap for the first real UnitLab verification flow.

This is the file to use for delivery tracking. Update phase status here as work lands.

Primary operator flow:
- import signal list;
- allocate channels;
- physically connect peripheral modules;
- prepare the selected rows with `Online 61850` when real MMS discovery is needed;
- select `n` signal rows;
- press `Run Test` once;
- receive live `tested` / `verified` / `failed` updates in the grid.

The operator should not have to choose the underlying test scenario in the normal flow.
Scenario selection, target planning, endpoint binding, network readiness checks, report subscription, online discovery, and evidence collection are system responsibilities.

## Product milestone

Milestone 1:
- one signal;
- one IED;
- one report;
- automatic verification;
- evidence-backed PASS/FAIL;
- Explain Why.

This is the first point where UnitLab behaves like a product instead of a set of contracts.

## Progress tracker

- [x] Phase A - VerificationTarget normalization
- [x] Phase B - SubscriptionPlan generation
- [x] Phase B.5 - Planner Validation
- [x] Phase C - Runtime session orchestration
- [x] Phase D - Evidence capture
- [x] Phase E - First single-signal auto verification
- [x] Phase F - Multi-signal same IED verification
- [x] Phase G - Multi-IED verification
- [x] Phase H - Recovery and reconnect verification
- [x] Phase I - Virtual-substation-backed automated regression testing
- [ ] Phase J - Real MMS integration

## Phase A

Purpose
- Turn selected signal-list rows into stable `VerificationTarget` objects.

Implementation
- Goal: normalize selected rows into a deterministic target payload.
- Scope: signal identity, endpoint/IED reference, expected feedback path, timeout/window, source row identity, coverage metadata.
- Out of scope: subscription planning, runtime orchestration, evidence capture, verdicts, recovery.
- Required models: `VerificationTarget`, selection snapshot/preview DTO, source row mapping.
- Required API endpoints: target preview endpoint from the backend product layer.
- Required backend services: target normalizer, row-to-target mapper, validation service.
- Required persistence: optional preview snapshot if the preview must be auditable.
- Required runtime integration: none beyond validating the data shape for later phases.
- Required UI changes: selection-driven preview panel for normalized targets and missing-field diagnostics.

Tests
- Unit tests: row-to-target mapping, ordering, missing-data validation.
- Integration tests: preview endpoint or service boundary.
- End-to-end tests: select rows in the grid and view the target preview.

Acceptance Criteria
- Same selected rows always produce the same target set.
- Every target can be traced back to its source row.
- The preview is runnable and visible to the operator.

Do Not Build Yet
- Auto subscription.
- Runtime session orchestration.
- Evidence or verdict logic.

Demo scenario
- User selects signal-list rows and sees a deterministic target preview.

Evidence
- Preview payload, UI preview panel, and passing target-normalization tests.

Rollback risk
- Wrong row-to-target mapping sends the wrong signal into later phases.

## Phase B

Purpose
- Turn normalized targets into grouped per-IED subscription plans.

Implementation
- Goal: group targets by IED/session/report control and choose candidate RCBs.
- Scope: per-IED plan groups, uncovered-target reasons, source classification, coverage summary.
- Out of scope: session connect/subscribe execution, evidence, recovery, verdict computation.
- Required models: `SubscriptionPlan`, group diagnostics, coverage summary, uncovered-target records.
- Required API endpoints: plan preview endpoint from the backend product layer.
- Required backend services: subscription planner, grouping service, report-control candidate resolver.
- Required persistence: plan snapshot if the plan needs to be replayed or audited.
- Required runtime integration: planner may read runtime metadata, but it must not execute runtime actions.
- Required UI changes: plan preview panel with grouped report controls and coverage.

Tests
- Unit tests: grouping, deterministic candidate selection, uncovered-target handling.
- Integration tests: planning against realistic target sets.
- End-to-end tests: select rows and view the plan preview.

Acceptance Criteria
- Same input targets produce the same plan.
- Every target is either covered or has a reason.
- The user does not manually configure RCB subscriptions.

Do Not Build Yet
- Subscription execution.
- Test action trigger.
- Evidence and verdict logic.

Demo scenario
- User selects rows spanning one or more IEDs and sees grouped plan output.

Evidence
- Plan preview, plan diagnostics, and planner test output.

Rollback risk
- Planner heuristics drift into nondeterministic or simulator-specific behavior.

## Phase B.5

Purpose
- Validate planner confidence before runtime starts.

Implementation
- Goal: show whether the plan is trustworthy enough to run.
- Scope: coverage score, source classification, report-control sanity checks, dataset and feedback-path consistency checks, uncovered-target visibility.
- Out of scope: runtime execution, subscriptions, evidence, verdicts.
- Required models: `PlannerConfidenceReport`, coverage breakdown, per-signal source classification, planner diagnostics.
- Required API endpoints: planner confidence report endpoint, or a confidence view attached to the plan preview endpoint.
- Required backend services: planner validator, consistency checker, confidence summary builder.
- Required persistence: optional plan-confidence snapshot if the report must be replayed later.
- Required runtime integration: discovery and SCD metadata checks only; no subscribe or trigger operations.
- Required UI changes: Planner Confidence Report panel with coverage percentage and per-signal origin labels.

Tests
- Unit tests: coverage calculation, consistency checks, source labeling.
- Integration tests: planner confidence over mixed discovery/SCD/fallback inputs.
- End-to-end tests: select rows, generate plan, inspect confidence, and confirm the pre-runtime view.

Acceptance Criteria
- The user can inspect planner confidence before any runtime action.
- Coverage is visible, including uncovered rows.
- Each signal shows where its plan came from: discovery, SCD, fallback, or uncovered.
- Suspicious RCB, dataset, or feedback-path combinations are flagged before runtime.

Do Not Build Yet
- Runtime orchestration.
- Evidence persistence.
- Verdict logic.

Demo scenario
- User sees a Planner Confidence Report such as coverage percentage and source labels for each signal.

Evidence
- Confidence report output, UI panel, and validator tests.

Rollback risk
- False confidence hides a bad plan and wastes later runtime effort.

## Phase C

Purpose
- Orchestrate live runtime sessions from the backend.

Implementation
- Goal: connect, subscribe, track state, and reconcile desired vs actual runtime state.
- Scope: backend-owned worker, connect/disconnect/reconnect ownership, session snapshots, runtime state transitions.
- Out of scope: final verdict polish, advanced recovery policy, multi-IED aggregation tuning.
- Required models: `SessionSnapshot`, runtime ownership metadata, orchestration state.
- Required API endpoints: run start, session state, reconcile/reconnect entrypoints.
- Required backend services: session manager, runtime orchestrator, reconcile loop, runtime adapter boundary.
- Required persistence: session state snapshots, ownership records, lifecycle events, subscription intent.
- Required runtime integration: MMS client connect/disconnect/reconnect, discovery, report subscription, generation tracking.
- Required UI changes: run status view with session state and subscribe progress.

Tests
- Unit tests: state transitions and reconcile decisions.
- Integration tests: simulator-backed connect and subscribe.
- End-to-end tests: start a run and observe the session reach reporting.

Acceptance Criteria
- The backend owns a live runtime session.
- Subscription state is visible and reproducible.
- Reconnect is runtime behavior, not a UI-only flag.

Do Not Build Yet
- Rich evidence semantics.
- Multi-IED batching polish.
- Operator verdict explanation beyond basic state.

Demo scenario
- User starts a run and sees the session reach reporting with active subscriptions.

Evidence
- Session snapshots, runtime events, integration traces, and orchestration API snapshots.

Rollback risk
- Product workflow logic leaks into the runtime layer and makes reconnect brittle.

## Phase D

Purpose
- Persist report observations as durable verification evidence.

Implementation
- Goal: map report observations into evidence rows with provenance, freshness, timing, and diagnostics.
- Scope: observation-to-evidence translation, append-only persistence, stale/timeout/invalid classification.
- Out of scope: full run orchestration polish, multi-IED aggregation, advanced recovery policy.
- Required models: `SignalVerificationEvidence`, `SignalVerificationEvidenceSet`, evidence diagnostics, evidence summary.
- Required API endpoints: run evidence and step detail endpoints.
- Required backend services: evidence repository, report-to-evidence mapper, evidence-set summarizer, diagnostic builder.
- Required persistence: evidence rows, evidence set record, per-run summary, diagnostics.
- Required runtime integration: report decoder output to evidence mapper, freshness and generation metadata.
- Required UI changes: evidence panel with per-signal rows, timestamps, freshness, and reason codes.

Tests
- Unit tests: evidence mapping, summary counts, stale/late/timeout classification, non-overwrite behavior.
- Integration tests: runtime report samples into evidence persistence.
- End-to-end tests: receive a report and see the persisted evidence row.

Acceptance Criteria
- Evidence rows explain what was observed, when, and why.
- Evidence survives reconnect and stale transitions.
- New evidence is appended, not rewritten.

Do Not Build Yet
- Full verdict orchestration for multi-signal runs.
- Advanced recovery policy.
- Multi-IED summary optimization.

Demo scenario
- A report arrives and the run detail view shows a persisted evidence row.

Evidence
- Evidence rows, evidence-set summary, and run detail output.

Rollback risk
- A later frame overwrites earlier evidence or erases stale context.

## Phase E

Purpose
- Deliver the first end-to-end automatic verification loop.

Implementation
- Goal: select one signal, trigger the test, receive confirmation, produce evidence, and produce a verdict.
- Scope: single-signal execution path, action trigger, step state machine, verdict explanation, run summary.
- Out of scope: same-IED batching, multi-IED fan-out, advanced recovery tuning.
- Required models: `VerificationRun`, `VerificationStep`, `VerificationTarget`, evidence set, diagnostic explanation.
- Required API endpoints: run create/start, trigger, run detail, verdict detail, evidence detail.
- Required backend services: execution service, action trigger service, step evaluator, verdict resolver, explanation builder.
- Required persistence: run record, step records, evidence rows, verdict state, execution diagnostics.
- Required runtime integration: trigger output action, await IEC 61850 confirmation, correlate report to the target.
- Required UI changes: one-click run action, live run state, verdict banner, Explain Why panel.
- Backend and frontend slice now implemented: single-signal auto-run endpoint, persisted run snapshot, verdict explanation response, and operator-facing Explain Why panel.

Tests
- Unit tests: verdict rules, step state transitions, explanation fields.
- Integration tests: simulator-backed runtime path.
- End-to-end tests: grid selection to run completion and verdict display.

Acceptance Criteria
- User can select one row, start a run, and get PASS or FAIL with evidence.
- The verdict is derived from persisted evidence, not UI state.
- The result explains why it is PASS or FAIL.
- The explanation includes fields such as output, expected path, observed path, IED, RCB, and latency.

Do Not Build Yet
- Multi-signal batching.
- Multi-IED orchestration details.
- Broad recovery analytics.

Demo scenario
- Select one signal, click run, and see a completed PASS or FAIL with Explain Why detail.

Evidence
- Run record, step record, evidence record, and verdict explanation panel.

Rollback risk
- Trigger-to-report correlation is wrong and produces a false PASS or false FAIL.

## Phase F

Purpose
- Run multiple signals on the same IED in one execution while preserving per-signal evidence.

Implementation
- Goal: reuse one session for same-IED targets and still keep separate step results.
- Scope: same-IED batching, shared session reuse, partial failure handling, group summary aggregation.
- Out of scope: cross-IED orchestration details, recovery workflow tuning, large-scale performance tuning.
- Required models: grouped `VerificationRun`, `VerificationStep`, grouped evidence summaries.
- Required API endpoints: reuse the single-run API with multi-target payloads and run detail queries.
- Required backend services: same-session step orchestrator, evidence correlator, grouped verdict aggregator.
- Required persistence: per-step evidence linkage, session-to-step mapping, summary counters.
- Required runtime integration: one session per IED with multiple selected signals mapped into one report flow.
- Required UI changes: grouped run summary on the same IED, per-step evidence rows.

Tests
- Unit tests: step grouping and shared-session correlation.
- Integration tests: multiple signals on one IED and one report session.
- End-to-end tests: same-IED multi-signal run from selected rows to verdict.

Acceptance Criteria
- Multiple signals on one IED execute in one run without losing per-signal evidence.
- One failed step does not erase the others.
- The UI shows one coherent run with separate step detail.

Do Not Build Yet
- Cross-IED fan-out policy changes.
- Recovery backoff policy.
- Fleet or batch scheduling abstractions.

Demo scenario
- User selects several rows on the same IED and runs them together.

Evidence
- Per-step evidence rows, grouped run summary, and same-session correlation output.

Rollback risk
- Shared session state causes one signal to overwrite another signal's evidence or verdict.

## Phase G

Purpose
- Support one selected group spanning multiple IEDs.

Implementation
- Goal: fan out to per-IED sessions while keeping one coherent product result.
- Scope: per-IED session fan-out, failure isolation, group-level aggregation, per-IED detail views.
- Out of scope: advanced recovery tuning, virtualization regression hardening, extreme scale optimization.
- Required models: multi-session `VerificationRun`, per-IED `SessionSnapshot`, aggregated evidence summaries.
- Required API endpoints: group-run status/detail endpoints, per-IED session detail endpoints.
- Required backend services: multi-session orchestrator, aggregation service, failure-isolation logic.
- Required persistence: one run record with multiple session snapshots and per-IED evidence trails.
- Required runtime integration: independent sessions for each IED, independent subscription and reconnect outcomes.
- Required UI changes: group run summary, per-IED status cards, explicit failure localization.

Tests
- Unit tests: multi-IED aggregation and failure isolation.
- Integration tests: two or more simulator-backed IEDs.
- End-to-end tests: one selected group spanning multiple IEDs.

Acceptance Criteria
- One IED can fail without corrupting the others.
- The run remains a single coherent product result.
- Per-IED evidence remains visible and separate.

Do Not Build Yet
- Sophisticated reconnect heuristics.
- Long-horizon retry policy.
- Fleet abstractions or distributed orchestration.

Demo scenario
- User selects a group spanning multiple IEDs and gets one coherent summary with per-IED detail.

Evidence
- Multiple session snapshots, isolated evidence trails, and aggregated verdict output.

Rollback risk
- Multiple IEDs collapse into one shared runtime state and hide localized failures.

## Phase H

Purpose
- Preserve desired work and evidence across disconnects, stale frames, report gaps, and reconnects.

Implementation
- Goal: keep evidence intact and reject stale generation updates.
- Scope: reconnect policy, generation rejection, stale evidence visibility, duplicate reconnect suppression, recovery-state display.
- Out of scope: distributed failover, lab-wide orchestration, advanced backoff tuning.
- Required models: `RecoveryState`, stale evidence markers, recovery diagnostics, session-generation metadata.
- Required API endpoints: reconnect action endpoint, recovery-state query, stale/evidence detail retrieval.
- Required backend services: recovery coordinator, reconnect deduper, generation guard, stale-frame rejection policy.
- Required persistence: recovery attempts, stale markers, retained evidence, generation history.
- Required runtime integration: generation protection, reconnect-capable sessions, report-health and freshness semantics.
- Required UI changes: recovery banner, stale-state indicators, reconnect action affordance.

Tests
- Unit tests: recovery-state transitions and stale-frame rejection.
- Integration tests: disconnect and reconnect the simulator-backed runtime.
- End-to-end tests: old-generation frames do not corrupt current evidence.

Acceptance Criteria
- Reconnect does not erase current or prior evidence.
- Stale and gap conditions are visible and explainable.
- Old-generation frames are rejected and diagnosable.
- One IED reconnect does not invalidate the others in the same selected group.

Do Not Build Yet
- Complex failover routing.
- Advanced retry analytics.
- Multi-cluster recovery abstractions.

Demo scenario
- Disconnect a virtual IED, reconnect it, and verify that the original evidence trail remains intact.

Evidence
- Recovery state, stale markers, rejected-frame diagnostics, and preserved evidence rows.

Rollback risk
- Reconnect resets current evidence or accepts an old-generation frame.

## Phase I

Status
- Completed.

Purpose
- Make the full verification flow reproducible in automated regression.

Implementation
- Goal: run the full workflow against a virtual substation with deterministic fixtures.
- Scope: scripted MMS server scenarios, golden report captures, failure injection, CI harness, scenario runner.
- Out of scope: hardware-only gating, distributed lab management, broad performance benchmarking.
- Required models: scenario definitions, expected captures, golden outputs, failure-injection descriptors.
- Required API endpoints: test-harness entrypoints or CLI commands.
- Required backend services: scenario runner, fixture loader, capture comparator, regression report generator.
- Required persistence: test artifacts, scenario outputs, golden captures, CI logs, artifact manifests.
- Required runtime integration: MMS client/server, discovery, reports, sessions, reconnect, generation protection, evidence capture, verdicts.
- Required UI changes: optional developer/test dashboard only if the harness is exposed in-app.

Tests
- Unit tests: scenario builders and capture comparators.
- Integration tests: virtual-substation scenarios.
- End-to-end tests: single-signal, same-IED, multi-IED, timeout, stale, and reconnect scenarios.

Acceptance Criteria
- The full flow can be exercised repeatedly in CI.
- The suite proves target normalization, planning, runtime orchestration, evidence capture, verdict computation, and recovery behavior.
- Failures are reproducible from saved fixtures and scenario definitions.

Do Not Build Yet
- Hardware-only gates.
- Distributed lab orchestration.
- Performance optimization beyond the regression harness need.

Demo scenario
- Run the regression suite and reproduce pass, timeout, stale, and reconnect scenarios.

Evidence
- CI artifacts, golden outputs, scenario logs, reproducible test results, and saved artifact bundles.

Rollback risk
- The harness becomes too synthetic and stops representing real MMS behavior.

## Phase J

Purpose
- Connect the now-stable product flow to real MMS-backed runtime sessions.

Implementation
- Goal: replace simulator-only verification execution with real MMS control and report handling for the already-proven product slices.
- Scope: real client/session wiring, target-to-endpoint reconciliation, report subscription/enable/disable, runtime fallback handling, endpoint resolution policy, production readiness checks, and host-agent-owned RJ45/IP/subnet/proxy configuration for the MMS path.
- Out of scope: reopening planner contracts, renaming evidence or verdict models, redesigning the product workflow, speculative fleet orchestration.
- Required models: the existing verification contracts plus any thin runtime adapter state needed for real endpoint sessions.
- Required API endpoints: verification run start/detail, runtime session control, recovery state, evidence detail, failure diagnostics.
- Required backend services: MMS-backed runtime adapter, session supervisor, report-control binder, reconnect/recovery coordinator.
- Required persistence: runtime session snapshots, evidence rows, recovery attempts, execution diagnostics.
- Required runtime integration: native MMS client/server, endpoint catalog or explicit host/port configuration, SCD-first model binding when available, discovery fallback when SCD is missing or incomplete, subscriptions, reconnect, generation protection, report delivery.
- Required UI changes: runtime source indicator, real-MMS status and diagnostics, agent-readiness warning, a short actionable hint if the host agent is not reporting a usable path, and a dedicated Network / RJ45 settings tab for IP/subnet/proxy configuration.

Tests
- Unit tests: adapter mapping, session state, report correlation, endpoint override policy.
- Integration tests: real MMS client/server smoke with a virtual substation, lib-server or iDiscover simulator, C264/BCU-focused endpoint fixtures.
- End-to-end tests: run the verified product flow against a real endpoint fixture or virtualized endpoint override and compare against the simulator-backed harness.

Acceptance Criteria
- The existing product flow can run against a real MMS endpoint without changing the user workflow contract.
- Evidence and verdict semantics remain stable when the transport changes from simulator to real MMS.
- Recovery and reconnect continue to preserve evidence and desired work.
- MMS host/port resolution is explicit and deterministic.
- The operator receives a clear preflight warning when the host agent does not report a usable path to the selected MMS target subnet.
- The product may suggest or auto-configure a network path from signal-list, allocation data, and host-agent state, but manual override remains available through the host-service-owned network settings flow.
- If a loaded SCD is present and matches the IED/access-point model, it is used first for candidate binding.
- If SCD is missing or incomplete, discovery is used to fill the gap after transport reachability exists.
- Discovery never invents the transport host; it only validates or enriches model data once a connection target exists.
- Test-only virtual endpoint override is available for MMS regression runs and can temporarily replace a real device IP with a virtual substation IP.
- The Phase J validation matrix should focus primarily on C264/BCU-style devices, because that is the dominant field target.

Do Not Build Yet
- New verdict semantics.
- Planner redesign.
- Fleet orchestration.

Demo scenario
- Repeat the verified single-signal and same-IED flows against a real MMS-backed virtual substation or a C264/BCU-style endpoint mapped to a virtual test IP.

Evidence
- Real endpoint session snapshots, report captures, preserved evidence, and pass/fail output from the live MMS path.
- Endpoint-resolution diagnostics showing whether the run used explicit host/port, SCD-first binding, or discovery fallback.
- Test-only override diagnostics showing when a virtual IP substituted a real device IP.

Rollback risk
- Real MMS wiring reintroduces transport-specific assumptions into the product flow or weakens the simulator-backed regression harness.

## Highest-risk implementation areas

- Correlating selected rows to targets and then to evidence without losing source-row identity.
- Session ownership and reconnect handling, especially generation rejection and stale-frame suppression.
- Multi-IED aggregation, where one shared state object would hide localized failures.
- Evidence persistence, especially preventing overwrite or rewrite on late updates.
- Operator-facing verdict explanation, because it is easy to hide the real reason behind a generic PASS/FAIL.
- Virtual substation fidelity, because a weak harness can make the whole flow look stable when it is not.

## Most likely architecture mistakes

- Letting the signal grid own execution truth instead of treating it as a workflow surface.
- Moving planner or verdict logic into the UI.
- Moving product workflow semantics into the C runtime.
- Collapsing target, plan, session, evidence, step, and run into one generic object.
- Introducing a separate execution subsystem too early instead of using one backend-owned orchestration path.
- Using a simulator shortcut as if it were a production guarantee.

## Where protocol logic can leak into product logic

- Parsing report fields directly inside verdict rules instead of mapping them through a runtime adapter.
- Reusing MMS field names as user-facing product concepts.
- Encoding sequence-number or generation behavior in the UI instead of in the runtime boundary.
- Making recovery policy depend on transport details in business services.

## Where product logic can leak into the C runtime

- Putting run, step, evidence, or verdict semantics into the client/server runtime code.
- Teaching the C layer how to decide PASS/FAIL or how to explain operator verdicts.
- Baking signal-list or allocation assumptions into MMS primitives.
- Adding UI-driven workflow states to the runtime session model.
