# IEC 61850 Self-Owned MMS Client Plan

Status: decision record and implementation roadmap. No self-owned MMS client is implemented yet.

This plan records the project decision that UnitLab may implement its own IEC 61850 MMS client if the required standards are available through project channels. Open-source stacks remain useful for comparison and simulator validation, but UnitLab core and report workflow must not become dependent on a single third-party MMS implementation.

## Decision

Build the IEC 61850 report runtime around a stable UnitLab adapter boundary:

```text
SCD + Signal List
  -> report subscription plan
  -> Iec61850ClientAdapter
  -> MMS client implementation
  -> real IED or lab simulator
```

The self-owned MMS client is one possible implementation of `Iec61850ClientAdapter`. It must remain replaceable by another implementation, such as IEC61850bean, libIEC61850 in a test-only tool, or a commercial stack, without changing the SCD parser, subscription planner, FAT evidence DTOs, or debug UI.

## Non-Negotiable Boundaries

- `frontend/src/modules/scd-sld-core` stays framework-neutral and does not know about MMS frames, sockets, native libraries, Java, or backend clients.
- `frontend/src/modules/iec61850-report-core` keeps portable DTOs, planning logic, report state semantics, diagnostics, and simulator-only validation.
- Backend/runtime owns real device sessions, command ordering, timeouts, reconnects, and evidence persistence.
- The self-owned MMS client must live behind an adapter boundary and must not leak BER, ACSE, COTP, or MMS PDU details into UnitLab domain models.
- Open-source or commercial stacks may be used as reference implementations, but not as hidden authorities for UnitLab business logic.
- GOOSE, Sampled Values, controls, file services, and IEC 62351 security are out of scope for the first self-owned MMS client milestone.

## Local IEC 61850 Documents

These are already present in the repository and are the first source of truth for IEC 61850 behavior:

| Document | Local path | Use |
| --- | --- | --- |
| IEC 61850-6:2024 | `/workspace/docs/.IEC61850/IEC 61850-6-2024.pdf` | SCL, IED, Communication, DataSet, ReportControl extraction |
| IEC 61850-7-2:2020 | `/workspace/docs/.IEC61850/IEC 61850-7-2-2020.pdf` | ACSI services, reports, DataSet, RCB attributes, GI, trigger options, optional fields |
| IEC 61850-8-1:2020 | `/workspace/docs/.IEC61850/IEC 61850-8-1-2020.pdf` | MMS mapping for IEC 61850 client/server communication |
| IEC 61850-7-3:2020 | `/workspace/docs/.IEC61850/IEC 61850-7-3-2020.pdf` | Common data classes and value shape interpretation |
| IEC 61850-7-4:2020 | `/workspace/docs/.IEC61850/IEC 61850-7-4-2020.pdf` | Logical nodes and data object definitions |
| IEC TR 61850-7-5:2021 | `/workspace/docs/.IEC61850/IEC TR 61850-7-5-2021.pdf` | Practical ACSI/report usage guidance |
| IEC TR 61850-7-500:2017 | `/workspace/docs/.IEC61850/IEC TR 61850-7-500-2017.pdf` | Object modeling and usage examples |

## Public Standards And References

These references are publicly available or have official free access paths. They are required to implement the lower protocol layers around MMS.

| Area | Reference | Link | Use |
| --- | --- | --- | --- |
| ISO-on-TCP | RFC 1006 | https://datatracker.ietf.org/doc/rfc1006/ | TPKT and ISO transport over TCP, port 102 |
| ISO-on-TCP update | RFC 2126 | https://datatracker.ietf.org/doc/rfc2126/ | RFC 1006 update/reference material |
| COTP / ISO transport | ITU-T X.224 | https://www.itu.int/ITU-T/recommendations/rec.aspx?id=3264 | Connection-mode transport protocol, equivalent to ISO/IEC 8073 |
| Session protocol | ITU-T X.225 | https://www.itu.int/rec/T-REC-X.225 | OSI session layer |
| Presentation service | ITU-T X.216 | https://www.itu.int/rec/T-REC-X.216 | Presentation service and contexts |
| Presentation protocol | ITU-T X.226 | https://www.itu.int/rec/T-REC-X.226 | Presentation protocol data units |
| ACSE service | ITU-T X.217 | https://www.itu.int/rec/T-REC-X.217 | Association control service definition |
| ACSE protocol | ITU-T X.227 | https://www.itu.int/rec/T-REC-X.227 | A-ASSOCIATE, A-RELEASE, A-ABORT protocol |
| ASN.1 notation | ITU-T X.680 | https://www.itu.int/rec/T-REC-X.680 | ASN.1 notation used by ACSE/MMS |
| ASN.1 BER | ITU-T X.690 | https://www.itu.int/rec/T-REC-X.690 | BER/CER/DER encoding rules |

## Documents To Obtain Through Project Channels

These are the important gaps for a strict from-standard MMS implementation.

| Document | Official link | Why needed |
| --- | --- | --- |
| ISO 9506-1:2003, Manufacturing Message Specification, Part 1: Service definition | https://www.iso.org/standard/37079.html | MMS abstract services, objects, service errors, and service semantics |
| ISO 9506-2:2003, Manufacturing Message Specification, Part 2: Protocol specification | https://www.iso.org/standard/37080.html | MMS PDUs, ASN.1 protocol model, request/response/confirmed service encoding |

Optional later documents:

- IEC 62351-3 and IEC 62351-4 for secure transport/authentication once plain MMS report workflow is proven.
- UCA IUG client/server conformance test procedures or implementation guidelines if accessible through project channels.
- Vendor-specific IEC 61850 implementation notes for the first real IED test matrix.

## Reference Implementations

These references are not normative, but they are useful for comparison, pcaps, and interoperability testing:

| Implementation | Link | Project use |
| --- | --- | --- |
| IEC61850bean / OpenIEC61850 | https://www.beanit.com/iec-61850/ | Apache 2.0 Java reference client/server candidate and comparison adapter |
| libIEC61850 | https://github.com/mz-automation/libiec61850 | GPLv3/commercial C stack; acceptable for local research/test tools, not embedded in closed UnitLab runtime without a licensing decision |
| Wireshark MMS/ACSE/COTP dissectors | https://www.wireshark.org/docs/dfref/m/mms.html | Packet inspection, pcap validation, field-level comparison |

## Implementation Slices

### Slice A - Protocol Research Pack

- Collect exact clauses/tables for RFC 1006, COTP, session, presentation, ACSE, ASN.1 BER, MMS, and IEC 61850-8-1 report mapping.
- Create golden pcaps from a known-good client talking to a simulator or lab IED.
- Record expected association, initiate, read, write, GI, report, release, and abort sequences.

Exit criteria:

- Every planned PDU has a source clause reference.
- Golden pcaps are decodeable in Wireshark.
- No UnitLab production code depends on a third-party MMS stack.

### Slice B - BER Codec

- Implement a small ASN.1 BER reader/writer.
- Support definite-length BER first.
- Cover tags, lengths, primitive/constructed values, booleans, integers, bit strings, octet strings, object identifiers, visible strings, UTC/generalized time if required by the mapped PDUs.
- Keep codec tests table-driven and portable to C#.

Exit criteria:

- Golden BER fixtures round-trip.
- Invalid tag/length/constructed forms fail with typed diagnostics.

### Slice C - TCP, TPKT, And COTP

- Open TCP to port 102.
- Implement RFC 1006 TPKT framing.
- Implement the COTP subset needed for IEC 61850 client connections.
- Support connect, data transfer, disconnect, segmentation/reassembly, and timeout diagnostics.

Exit criteria:

- Client reaches a valid COTP connection against a reference server.
- Disconnect and malformed frame tests are deterministic.

### Slice D - Session, Presentation, And ACSE

- Implement the session and presentation subset used by IEC 61850 MMS.
- Negotiate presentation contexts required for ACSE and MMS.
- Implement ACSE associate, release, and abort handling.

Exit criteria:

- Client establishes and releases an association against a reference server.
- Rejected association and abort paths return structured diagnostics.

### Slice E - MMS Initiate And Basic Services

- Implement MMS initiate request/response.
- Implement confirmed request/response correlation.
- Implement the minimum services required for:
  - server/model directory discovery;
  - named variable read;
  - named variable write.

Exit criteria:

- Client can read one known object from a simulator or lab IED.
- Request invoke IDs, response correlation, errors, and timeouts are covered.

### Slice F - IEC 61850 ReportControl Access

- Map IEC 61850 object references to MMS variable access paths according to IEC 61850-8-1.
- Read RCB attributes needed by the current UnitLab precheck:
  - `RptID`;
  - `RptEna`;
  - `DatSet`;
  - `ConfRev`;
  - `OptFlds`;
  - `TrgOps`;
  - `BufTm`;
  - `IntgPd`;
  - `Owner`;
  - reservation-related attributes available for the RCB kind.
- Write allowed RCB attributes only while disabled.

Exit criteria:

- Backend adapter can read live RCB state and compare it with an SCD-derived plan.
- Blocking precheck errors prevent reserve/enable/GI.

### Slice G - Report Activation And GI

- Reserve or claim the selected RCB instance according to the RCB kind and live state.
- Enable reporting through `RptEna`.
- Trigger GI.
- Decode incoming MMS `InformationReport` into the existing `Iec61850ReportEvent` DTO shape.
- Route normalized values through the existing signal observation mapper.

Exit criteria:

- One selected Signal List row can be proven through one real or simulated report.
- Enable, GI, disable, and release produce auditable backend events.

### Slice H - Robustness And Interop

- Handle indexed BRCBs.
- Handle `EntryID`, `SqNum`, `TimeOfEntry`, `BufOvfl`, reason codes, optional data references, and subset reports.
- Add reconnect and cleanup behavior for disconnect while enabled, rejected writes, stale `ConfRev`, access denied, and malformed report payloads.
- Test against at least two independent IED/server implementations before calling the adapter production-ready.

Exit criteria:

- Failures produce explicit UnitLab diagnostics and evidence.
- No failure path silently marks FAT signal verification as successful.

## Validation Strategy

- Unit tests for BER codec and protocol frame encode/decode.
- Golden pcap replay tests for association, MMS initiate, RCB read/write, GI, report, release, and abort.
- Wireshark comparison for field-level protocol validation.
- Parity tests against IEC61850bean or libIEC61850 as reference behavior.
- Simulator tests for deterministic fault injection.
- Lab IED tests before enabling any operator-facing real-device mode.

## Runtime And Safety Risks

- A self-owned MMS client can fail in ways that look like an IED failure. Diagnostics must distinguish transport failure, association failure, MMS service error, RCB access error, and report decode error.
- Report enablement is hardware-facing runtime behavior. It must stay backend-owned, explicit, logged, and recoverable.
- FAT evidence must never depend on transient frontend state or unverified adapter assumptions.
- Real-device mode must be visually distinct from dry-run/simulator validation.
- GPL code must not be copied into UnitLab closed runtime unless a separate licensing decision is made.

## Current Project Position

- Implemented today: SCD report inventory, Signal List merge, subscription plan builder, simulator-only report runtime, normalized report event DTOs, observation mapping, backend simulator parity, incoming report routing, activation precheck gates, MMS endpoint catalog, external IED simulator fixture export, external IED simulator process scaffold, fixture parser/model materialization, and report option records.
- Not implemented today: real MMS transport, self-owned MMS client, real IED connection, report persistence, GOOSE, Sampled Values, and IEC 62351 security.
- Implemented boundary step: backend has an explicit MMS endpoint catalog, a fail-closed unavailable MMS adapter, a JSON fixture boundary for an external IED simulator, a C/CMake simulator process scaffold, materialized fixture records, and report option records.
- Next architecture step: implement the libIEC61850 server model loader behind that process scaffold, then connect UnitLab to it through the same backend report-runtime contract.
