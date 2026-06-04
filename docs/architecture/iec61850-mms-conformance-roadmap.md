# IEC 61850 MMS Conformance Roadmap

Status: rebaselined implementation roadmap. This tracks the MMS-first slice order for UnitLab-owned IEC 61850 protocol work.

See also:
- [UnitLab IEC 61850 MMS Core Boundary](./iec61850-unitlab-mms-core-boundary.md)
- [UnitLab MMS Semantic Contract](./iec61850-unitlab-mms-semantic-contract.md)
- [UnitLab MMS Layered Architecture](./iec61850-unitlab-mms-layered-architecture.md)
- [IEC 61850 Self-Owned MMS Client Plan](./iec61850-self-owned-mms-client-plan.md)

## Scope

This roadmap is intentionally MMS-only.

In scope:
- TCP connection to MMS on port 102.
- RFC 1006 / TPKT.
- COTP connection request / confirm.
- ISO Session / Presentation.
- ACSE AARQ / AARE.
- MMS InitiateRequest / InitiateResponse.
- Optional Identify discovery.
- GetNameList discovery for domains, variables, and named variable lists.
- GetVariableAccessAttributes.
- Read for data, RCBs, and dataset members.
- Reporting setup and report reception.
- Write for ordinary attributes, RCB attributes, and control model fields.
- Control semantics for direct operate and SBO paths.

Out of scope for this roadmap:
- GOOSE.
- Sampled Values.
- File services.
- IEC 62351 security.

## Comparison Rule

`libiec61850` is the external reference oracle for this roadmap.

The implementation is not considered complete for a slice until all of the following are true:
- the focused UnitLab tests pass;
- a golden frame, pcap, or Wireshark-captured dump exists for the slice;
- the observed behavior matches the reference oracle for the supported path;
- any divergence is documented as either a deliberate UnitLab choice or an unresolved gap.

Reference comparison does not mean copying implementation details. It means matching behavior at the service, frame, and diagnostic level for the supported MMS surface.

For slice 0, the current golden source is the reference terminal capture at `/workspace/tools/iec61850_reference/captures/mms Areva746.pcap`, with `libiec61850` used to cross-check the same layer boundaries when a capture is ambiguous.

## Slice Plan

### Slice 0 - Transport And Association Bring-Up

Implement the minimum vertical path from TCP connect through MMS initiate.

Deliverables:
- TCP connect to port 102;
- RFC 1006 / TPKT framing;
- COTP connection request / confirm;
- ISO Session / Presentation framing;
- ACSE AARQ / AARE;
- MMS InitiateRequest / InitiateResponse;
- exact-byte golden transport and handshake captures for the supported path;
- reference-capture handshake bytes from `mms Areva746.pcap` as the current compatibility oracle.

Current implementation status:
- ACSE AARE assembly now uses a standards-shaped field sequence with explicit protocol version, application context, result, result-source-diagnostic, and user-information EXTERNAL wrapping the InitiateResponse.
- Wire-foundation and native-wire smoke currently pass on the updated handshake path.

Exit criteria:
- a reference endpoint can complete the full connection and initiate exchange;
- the slice has focused tests plus a Wireshark-visible capture or pcap;
- the supported handshake bytes are compared against a recorded golden frame, not reconstructed ad hoc.

### Slice 1 - Discovery And Basic Read

Implement the first useful MMS discovery and read surface.

Deliverables:
- optional Identify discovery;
- GetNameList for domains, variables, and named variable lists;
- GetVariableAccessAttributes;
- Read for data objects, RCBs, and dataset members;
- service-kind classification for these requests and responses.

Current implementation status:
- Read request framing now uses the standard `ReadRequest -> variableAccessSpecification -> listOfVariable -> ObjectName` shape through the wire layer and semantic bridge.
- GetNameList domain discovery now supports `NamedVariable` domain browse in addition to logical-device and data-set browsing.
- GetVariableAccessAttributes request framing is wired through the wire layer and semantic bridge.
- Focused unit tests cover the new `NamedVariable` browse collector and the server-runtime response path.
- Live metadata-probe capture for the new browse path is still gated behind `UNITLAB_IEC61850_SIM_WITH_LIBIEC61850=ON`; the current workspace build is not linked against libIEC61850.

Exit criteria:
- known MMS objects can be discovered and read through the supported path;
- unsupported object shapes return typed diagnostics instead of generic failure;
- the slice has a reference capture and focused UnitLab tests.

### Slice 2 - Reporting Setup

Implement the report-control setup path that real MMS workflows depend on.

Deliverables:
- Read RCB;
- Write Resv / ResvTms or Owner, depending on BRCB or URCB usage;
- Write TrgOps / OptFlds / IntgPd / DatSet when required;
- Write GI = true when general interrogation is needed;
- Write RptEna = true;
- explicit metadata validation for supported report-control fields.

Exit criteria:
- the runtime can configure a report-control path end to end;
- setup failures are explicit and traceable;
- the slice has a reference capture and focused UnitLab tests.

### Slice 3 - Reports

Implement incoming report reception and normalization.

Deliverables:
- InformationReport / unconfirmed server PDU decode;
- report acceptance and rejection handling;
- expected-vs-actual capture for report evidence;
- evidence-friendly runtime events and timestamps.

Exit criteria:
- an incoming report can be normalized into a UnitLab evidence record;
- malformed or unexpected report payloads fail closed;
- the slice has a reference capture and focused UnitLab tests.

### Slice 4 - Write Path

Implement the write surface for ordinary attributes and MMS-managed control data.

Deliverables:
- ordinary writable attributes;
- RCB attribute writes;
- control model field writes;
- explicit write diagnostics for unsupported attributes and types.

Exit criteria:
- supported writes are explicit and testable;
- unsupported writes fail with typed diagnostics;
- the slice has a reference capture and focused UnitLab tests.

### Slice 5 - Control Semantics

Implement control-model semantics only if the product scope requires them for MMS completeness.

Deliverables:
- direct operate;
- select-before-operate;
- select-with-value where needed;
- cancel if needed;
- control failure diagnostics;
- reference comparison for control request and response paths.

Exit criteria:
- supported control flows are explicit and testable;
- unsupported control semantics remain documented and blocked;
- the slice has a reference capture and focused UnitLab tests.

### Slice 6 - Interoperability Hardening

Close the gaps that only show up under real interoperability pressure.

Deliverables:
- malformed and truncated frame matrix;
- duplicate, stale, out-of-order, and timeout cases;
- supported-profile documentation that names the exact implemented MMS surface;
- second-endpoint comparison where available.

Exit criteria:
- the supported profile is reproducible from tests and docs;
- every known unsupported edge is explicitly documented;
- the slice has a reference capture and focused UnitLab tests.

## Slice Closure Rules

Each slice closes only when all of these are true:
- focused package tests pass;
- a reference comparison against `libiec61850` is recorded for the supported path;
- a Wireshark-readable capture or pcap exists for the supported path;
- any new diagnostic or behavioral gap is documented;
- the roadmap is updated with the slice result and remaining work.

## Validation Expectations

- Unit tests for encode/decode, discovery, read, write, report setup, reports, correlation, diagnostics, and runtime transitions.
- Golden pcap or byte-level frame comparison for every supported protocol path.
- Reference comparison against `libiec61850` for supported service behavior, plus captured golden frames for any slice that closes before a live parity harness exists.
- For slice 0, prefer reference-capture handshake bytes as the first golden source and keep the captured frame hex under version control.
- Wireshark verification of the handshake and service frames for each slice.
- Malformed-input tests for truncated frames, unsupported tags, invalid lengths, and rejected negotiation.

## Risks

- Reference parity can hide a UnitLab bug if the comparison harness is too narrow.
- Over-expanding the slice surface before the lower layers are stable will produce false confidence.
- If unsupported behavior is not documented per slice, the implemented profile will drift from what operators can safely rely on.
