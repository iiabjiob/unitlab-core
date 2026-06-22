# IEC 61850 UnitLab Product Roadmap

Status: living roadmap for turning the native IEC 61850 MMS stack into a production-shaped UnitLab workflow.

This roadmap sits above the protocol/client/server audit:
- protocol/client/server detail lives in `docs/iec61850-native-runtime-audit.md`;
- lower-level client discovery/report work stays in `docs/mms-client-roadmap.md`.

The goal here is the UnitLab product flow:
- signal list with IEC 61850 endpoint references;
- optional SCD as a functional hint, not a hard dependency;
- automatic grouping of signals into per-device subscription plans;
- automatic selection of candidate report controls;
- automatic subscribe / reconnect / recovery;
- report-based confirmation that a simulated output actually reached the target IED;
- verdicts based on real IEC 61850 feedback and timing windows.

## Product target

The library and runtime must support this flow without engineer-heavy setup:

- User selects a group of signals in the signal list.
- Signals may span multiple IEDs.
- UnitLab derives the device/report plan in the background.
- UnitLab subscribes to the right report controls.
- UnitLab triggers outputs.
- The target IED reports back through MMS.
- UnitLab marks the test step as:
  - `confirmed` when the report is received and valid;
  - `verified` when the feedback returned through IEC 61850 and is inside the timing window;
  - `stale` / `timed_out` / `unconfirmed` / `failed` when evidence is missing or invalid.

## Current strengths

- Native MMS association, discovery, RCB selection, report decoding, reconnect, freshness, and per-RCB sequence hygiene are already strong enough to use as the foundation.
- The lower runtime no longer treats reconnect as a business intent.
- Signal freshness is explicit and stale signals keep the last known good value.
- Report health now influences live-cache staleness.
- The server side can emit the report shape needed for the current flow.
- The system already has useful protocol-level tests and golden-capture pressure points.

## Current product gaps

- No first-class planner yet turns signal-list rows into a subscription plan across one or more IEDs.
- SCD is still a hint path, not a fully integrated optional input layer for the product workflow.
- Test-step confirmation and verdict logic are not yet a first-class contract above the runtime.
- The product does not yet have a visible runtime model for `confirmed`, `verified`, `timed_out`, `unconfirmed`, and `stale`.
- Multi-IED coordination is not yet a product-level operating model.
- Evidence reconstruction still leans too much on runtime state instead of a durable product trace.

## Roadmap structure

The roadmap is split into PR-sized slices that can be tracked and checked off in order.

Rules:
- each PR should be small enough to review safely;
- each PR should have one ownership boundary;
- no workaround layers or duplicate orchestration paths;
- do not add a new subsystem unless the current boundary is clearly insufficient;
- keep the lower IEC 61850 layer reusable, not UnitLab-specific.

## PR-by-PR plan

### PR1 - Signal list to requested verification targets

Status: in progress.

Goal:
- Convert selected signal-list rows into normalized verification targets.
- The target normalization lives in the UnitLab planner layer, using the 61850 model plan only as source input.

Must do:
- normalize each selected row into a target object;
- include target signal identity;
- include IEC 61850 address;
- include endpoint / IED reference;
- include expected feedback path;
- include timeout or verification window;
- keep one signal-to-one target mapping explicit even when a later planner groups them.

Must not do:
- do not subscribe yet;
- do not invent planner heuristics in the UI;
- do not require SCD as a hard dependency;
- do not lose source row identity.

Done when:
- selected signal-list rows become stable verification-target objects;
- each target can be inspected in tests and logs;
- targets carry enough data for later planning without re-reading the UI.

### PR2 - Subscription planner

Status: in progress.

Goal:
- Turn requested verification targets into per-IED / per-RCB subscription plans.
- The current planner uses the 61850 model plan for exact dataset matches, with a safe single-report fallback and explicit uncovered targets.

Must do:
- accept a list of requested targets;
- group targets by IED / endpoint / session;
- choose candidate RCBs;
- explain each choice with a reason;
- split uncovered targets into explicit reasons;
- label each target source as `from SCD`, `from discovery`, `fallback`, or `not found`;
- current slice may only resolve `from SCD`, `fallback`, and `not found` until a discovery snapshot resolver is wired in.

Must not do:
- do not execute MMS writes in the planner;
- do not hide unsupported targets;
- do not turn fallback logic into silent magic;
- do not merge unrelated IEDs into one plan.

Done when:
- input targets produce a deterministic subscription plan;
- every target is either covered or has a reason;
- one plan can span multiple IEDs cleanly.

### PR3 - Verification evidence model

Status: not started.

Goal:
- Introduce a durable evidence object for signal verification.

Must do:
- add `SignalVerificationEvidence`;
- store `signal_id`;
- store `expected_path`;
- store `actual_report_path`;
- store `source_ied`;
- store `rpt_id`;
- store `dataset`;
- store `received_at`;
- store `latency_ms`;
- store `quality`;
- store `freshness`;
- store `verdict`;
- store `reason`;
- preserve provenance across reconnect and stale transitions.

Must not do:
- do not flatten evidence into a boolean;
- do not make the UI authoritative;
- do not destroy original evidence when a later update arrives;
- do not add historian semantics yet.

Done when:
- evidence can be created, updated, and inspected independently of UI state;
- verification verdicts can be explained from stored fields.

### PR4 - First auto test flow

Status: not started.

Goal:
- Implement the simplest end-to-end verified flow.

Must do:
- activate UnitLab output;
- wait for IEC 61850 confirmation;
- compare against the allowed time window;
- mark the signal as `tested`, `verified`, `failed`, or `timeout`;
- use the evidence model instead of ad hoc booleans.

Must not do:
- do not require manual per-device tweaking for the happy path;
- do not introduce fleet orchestration yet;
- do not shortcut report confirmation with local UI-only state.

Done when:
- a single simulated output can be verified against a real report path;
- time-window violations are visible;
- the verdict is backed by recorded evidence.

### PR5 - Multi-IED selected group flow

Status: not started.

Goal:
- Support one selected signal group spanning multiple IEDs.

Must do:
- split the group into multiple device/session plans;
- keep each session isolated;
- preserve verdict evidence per IED;
- keep the user-visible result coherent for the whole group.

Must not do:
- do not collapse all sessions into one shared runtime;
- do not lose per-IED failure detail;
- do not regress single-IED behavior.

Done when:
- one selected group can produce multiple subscription sessions;
- each IED can confirm or fail independently;
- the product still presents one coherent test result.

### PR6 - Hardening

Status: not started.

Goal:
- Harden the end-to-end verification path.

Must do:
- handle reconnect during test;
- handle stale signals;
- handle report gaps;
- handle partial failures;
- surface useful diagnostics;
- keep late frames from corrupting current evidence.

Must not do:
- do not add fleet abstractions before the workflow is stable;
- do not hide protocol failure under generic success;
- do not weaken evidence traceability.

Done when:
- the first auto test flow survives real disconnect/reconnect edges;
- stale and gap conditions are visible and non-ambiguous;
- partial failures remain explainable.

## PR tracking checklist

- [ ] PR1 - Signal list to requested verification targets
- [ ] PR2 - Subscription planner
- [ ] PR3 - Verification evidence model
- [ ] PR4 - First auto test flow
- [ ] PR5 - Multi-IED selected group flow
- [ ] PR6 - Hardening

## What should be treated as mandatory

- Signal-list to IED/report planning must work without manual engineer tuning.
- SCD must remain optional and functional, not mandatory.
- Report confirmation must be part of the verdict path.
- Reconnect must preserve desired work, not just transport state.
- Late or stale frames must not corrupt current evidence.
- The product must explain why a signal was marked confirmed, verified, stale, or failed.

## What is desirable but not mandatory yet

- Richer report anomaly metrics.
- More explicit discovery snapshot/versioning.
- More server-side typed-node breadth for rare MMS shapes.
- Better multi-session diagnostics.
- Async worker cancellation refinements.
- Broader golden-capture automation.

## Related documents

- `docs/iec61850-native-runtime-audit.md`
- `docs/mms-client-roadmap.md`
