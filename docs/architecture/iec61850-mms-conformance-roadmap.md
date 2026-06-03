# IEC 61850 MMS Conformance Roadmap

Status: implementation roadmap. This roadmap tracks the MMS-only slice plan for UnitLab-owned IEC 61850 protocol work.

See also:
- [UnitLab IEC 61850 MMS Core Boundary](./iec61850-unitlab-mms-core-boundary.md)
- [UnitLab MMS Semantic Contract](./iec61850-unitlab-mms-semantic-contract.md)
- [UnitLab MMS Layered Architecture](./iec61850-unitlab-mms-layered-architecture.md)
- [IEC 61850 Self-Owned MMS Client Plan](./iec61850-self-owned-mms-client-plan.md)

## Scope

This roadmap is intentionally MMS-only.

In scope:
- Association, release, abort, and reject handling.
- Initiate, confirmed request/response correlation, and service classification.
- Basic data access: read, write, and browsing/name-list discovery.
- Dataset and report-control metadata access.
- Report reception, GI, and report evidence routing.
- Runtime diagnostics, request correlation, and negative-path handling.

Out of scope for this roadmap:
- GOOSE.
- Sampled Values.
- File services.
- IEC 62351 security.

## Comparison Rule

`libiec61850` is the external reference oracle for this roadmap.

The implementation is not considered complete for a slice until all of the following are true:
- the focused UnitLab tests pass;
- a golden frame or pcap comparison exists for the slice;
- the observed behavior matches the reference oracle for the supported path;
- any divergence is documented as either a deliberate UnitLab choice or an unresolved gap.

Reference comparison does not mean copying implementation details. It means matching behavior at the service, frame, and diagnostic level for the supported MMS surface.

## Slice Plan

### Slice 0 - Protocol Boundary Freeze

Status: closed.

Lock the lower-layer ownership split before expanding the service surface.

Deliverables:
- stable `BER`, `TPKT`, `COTP`, `session`, `presentation`, `ACSE`, and `MMS PDU` boundaries;
- explicit semantic-result and diagnostic mapping;
- golden fixtures for encode/decode round-trips and malformed frames;
- a `libiec61850`-captured golden association-accept frame asserted by the native smoke test.

Exit criteria:
- no wire-layer code mutates runtime state directly;
- the handshake path is covered by a native smoke test and a `libiec61850`-derived golden accept frame.

### Slice 1 - Association Lifecycle

Implement association, release, abort, and reject handling end to end.

Progress:
- release request/response now maps through the wire-semantic bridge into the runtime session state machine;
- conclude error now projects into runtime abort handling so the lifecycle has an explicit error exit;
- runtime bridge tests cover association, release, and conclude-error abort transitions;
- wire codec tests cover `CONCLUDE_*` PDU round-trips.

Deliverables:
- association request/accept/reject framing;
- release and abort flows;
- timeout and malformed-handshake diagnostics;
- parity tests against `libiec61850` for the association lifecycle.

Exit criteria:
- the stack can establish and tear down a session against a reference endpoint;
- failures are explicit and traceable.

### Slice 2 - Initiate And Correlation

Implement MMS initiate and the request-correlation layer that all confirmed services share.

Deliverables:
- initiate request/response encoding and decoding;
- invoke-ID allocation and correlation;
- pending-request lifecycle handling;
- confirmed request/response classification and error mapping.

Exit criteria:
- a confirmed request can be matched to the correct response or timeout;
- the slice has a pcap or frame comparison against `libiec61850`.

### Slice 3 - Basic Data Access

Implement the minimum useful data-access surface for real MMS work.

Deliverables:
- named variable read;
- named variable write;
- browsing/name-list discovery;
- basic object-reference normalization and service-kind classification.

Exit criteria:
- a known object can be read and written through the supported path;
- unsupported object shapes return typed diagnostics instead of generic failure.

### Slice 4 - Dataset And Report-Control Metadata

Implement the metadata surface that report/runtime workflows depend on.

Deliverables:
- dataset discovery and member enumeration;
- RCB metadata read;
- reserve/enable/disable/release transitions;
- GI preconditions and metadata validation.

Exit criteria:
- the runtime can inspect a report-control path end to end;
- live metadata matches the reference oracle for the supported subset.

### Slice 5 - Report Reception And Evidence

Implement incoming report normalization and evidence capture.

Deliverables:
- `InformationReport` decode;
- report acceptance and rejection handling;
- expected-vs-actual value capture;
- evidence-friendly runtime events and timestamps.

Exit criteria:
- an incoming report can be normalized into a UnitLab evidence record;
- malformed or unexpected report payloads fail closed.

### Slice 6 - Control Semantics

Implement control-model semantics only if the product scope requires them for MMS completeness.

Deliverables:
- direct operate;
- select-before-operate;
- cancel;
- control failure diagnostics;
- reference comparison for control request/response paths.

Exit criteria:
- supported control flows are explicit and testable;
- unsupported control semantics remain documented and blocked.

### Slice 7 - Interoperability Hardening

Close the gaps that only show up under real interoperability pressure.

Deliverables:
- malformed and truncated frame matrix;
- duplicate, stale, out-of-order, and timeout cases;
- second-endpoint reference comparison where available;
- supported-profile documentation that names the exact implemented MMS surface.

Exit criteria:
- the supported profile is reproducible from tests and docs;
- every known unsupported edge is explicitly documented.

## Slice Closure Rules

Each slice closes only when all of these are true:
- focused package tests pass;
- a reference comparison against `libiec61850` is recorded for the supported path;
- any new diagnostic or behavioral gap is documented;
- the roadmap is updated with the slice result and remaining work.

## Validation Expectations

- Unit tests for encode/decode, correlation, diagnostics, and runtime transitions.
- Golden pcap or byte-level frame comparison for every supported protocol path.
- Reference comparison against `libiec61850` for supported service behavior, plus captured golden frames for any slice that closes before a live parity harness exists.
- Malformed-input tests for truncated frames, unsupported tags, invalid lengths, and rejected negotiation.

## Risks

- Reference parity can hide a UnitLab bug if the comparison harness is too narrow.
- Over-expanding the slice surface before the lower layers are stable will produce false confidence.
- If unsupported behavior is not documented per slice, the implemented profile will drift from what operators can safely rely on.
