# IEC 61850 Self-Owned MMS Client Plan

Status: decision record and implementation roadmap. A first single-device native MMS client path exists for association, root discovery from endpoint host/port, RptEna, GI, normalized report-value ingestion, and live debug-page refresh while associated; it is still pre-production and not yet a multi-device client. External MMS discovery now starts with the live VMD logical-device list when no domain is supplied, so an IP or network name is enough for the first discovery step. The persistent external MMS client can also start without an SCD path, browse first, auto-select the first discovered report-control candidate, and then execute `RptEna`/`GI` against the live selection. Root live discovery retains every discovered logical-device domain and then discovers each domain in the same native client session instead of selecting only the first domain. Live-discover flows use the MMS RCB instance names returned by discovery, including indexed BRCB/URCB report controls such as `brcbA01` and `urcbA01`, and enrich live report candidates by reading individual RCB fields such as `RptID`, `DatSet`, `ConfRev`, `BufTm`, `IntgPd`, `OptFlds`, and `TrgOps`. `OptFlds` and `TrgOps` are exposed both as raw MMS bit-string diagnostics and typed booleans in the backend/UI state contract, and the debug client page displays the effective selected RCB options read-only. Confirmed Write responses with access-result failures are treated as command failures rather than optimistic success. Structured FCDA report values are currently projected to their primary value leaf for operator-facing signal state instead of surfacing raw BER hex blobs. The debug UI currently polls the backend state endpoint during active association so the backend can drain async native reports into the UI contract; a dedicated event stream remains a later production hardening slice.

This plan records the project decision that UnitLab will implement its own IEC 61850 MMS client and simulator flow. Open-source stacks remain useful as reference implementations and interoperability oracles, but UnitLab core, simulator, and report workflow must not become dependent on a single third-party MMS runtime.

See also: [UnitLab IEC 61850 MMS Core Boundary](./iec61850-unitlab-mms-core-boundary.md).
See also: [UnitLab IEC 61850 MMS Client Boundary](./iec61850-unitlab-mms-client-boundary.md).
See also: [UnitLab MMS Semantic Contract](./iec61850-unitlab-mms-semantic-contract.md).
See also: [UnitLab MMS Layered Architecture](./iec61850-unitlab-mms-layered-architecture.md).
See also: [IEC 61850 MMS Conformance Roadmap](./iec61850-mms-conformance-roadmap.md).

## Current MMS / IEC 61850 Gap Audit Against IEDScout-Class Behavior

Scope: this audit covers generic IEC 61850-8-1 MMS client/server behavior needed for an IEDScout-like engineering workflow. It intentionally excludes vendor-specific quirks, private data models, proprietary services, and GOOSE/Sampled Values.

### Current coverage

- Client association is implemented for the basic MMS-over-TCP path used by the native wire client.
- Client live discovery can enumerate logical devices, logical nodes, data sets, data-set members, and BRCB/URCB report-control candidates for the current single-device flow.
- External MMS discovery can start from only host/port: the native client first asks the live VMD logical-device list and then discovers every returned live domain in one native session.
- Persistent external MMS browse-only startup can omit the explicit IED name; the backend and native client infer the live device identity from the browse response when enough naming context is present.
- Live-discovered report candidates are enriched from individual RCB reads rather than SCD-derived `RptID`, `DatSet`, `ConfRev`, `BufTm`, or `IntgPd` values.
- Live-discovered `OptFlds` and `TrgOps` are decoded into typed `optional_fields` and `trigger_options` so the UI can show effective report options instead of only raw bit strings.
- Client can connect from SCD/in-memory model or external MMS browse-only mode without live discovery and can subscribe to a known RCB path once a live RCB is selected.
- Client can write RCB `OptFlds`, `TrgOps`, `RptEna`, and `GI` and treats failed confirmed Write responses as command failures.
- Client can ingest asynchronous InformationReport frames, map `RptID`, `DatSet`, `ConfRev`, sequence, reason-code, data references, primary values, quality, and timestamps into the debug runtime contract.
- Server/simulator can expose a usable model, answer core discovery reads, process selected RCB writes, emit GI reports, and emit data-change/quality-change/data-update reports for the current SCD-shaped data model.
- The debug backend keeps a current signal-state projection so sporadic reports can update last-known state without requiring a full GI report each time.

### Client gaps before IEDScout-class parity

| Area | Current state | Gap to close |
| --- | --- | --- |
| Association lifecycle | Basic associate/receive/write flow exists. | Add complete MMS lifecycle handling for release/conclude/abort paths, reconnect behavior, explicit association state diagnostics, and negotiated capability/size limits. |
| Discovery completeness | Single-device discovery covers the subset needed for current reports. | Implement robust object-class discovery across multiple logical devices, all logical nodes, named variable lists, data objects, data attributes, and indexed/non-indexed RCB instances without relying on SCD shortcuts. |
| Multi-domain discovery | Root discovery retains and exposes every live logical-device domain, then discovers each domain in one native session. | Harden cross-domain UI selection and continue toward complete namespace browsing across domains. |
| GetNameList pagination | Large responses are buffered, but pagination is not yet a proven contract. | Implement and validate `moreFollows`/continue-after handling for every discovered object class. |
| Typed model building | Reports are projected to primary signal states. | Build a full typed IEC 61850 model from MMS discovery: DO/DA tree, FC, CDC-like structure hints, leaf types, array/structure nesting, and display references. |
| Read services | Targeted reads and RCB preflight exist. | Add general read-by-reference and bulk read workflows for arbitrary discovered objects, including structured values, arrays, qualities, timestamps, and clear per-item errors. |
| Write services | Basic bool/uint/int/string/hex writes exist. | Add typed write helpers over the discovered model so callers do not manually encode BER tags for normal IEC 61850 data attributes. |
| Data-set operations | Data-set discovery and member mapping exist for the current flow. | Add full named variable list handling, data-set read validation, dynamic data-set creation/deletion where supported, and strict `DatSet`/member compatibility checks. |
| Report controls | Live discovery can enumerate BRCB and URCB candidates and preserve their MMS class. | Complete runtime parity: reservation rules, `Resv`, `ResvTms`, owner handling, purge buffer, entry-id start/resume, buffer overflow semantics, integrity period, indexed instance selection, and disabled-before-configuration validation. |
| Report decoding | GI and data-change reports are decoded into current signal state. | Decode full report optional-field combinations, inclusion bitstrings, partial reports, segmentation/large reports, multi-reason values, duplicate/out-of-order sequence behavior, and unknown member diagnostics. |
| Trigger/options profile | Backend writes a default live SCADA profile before `RptEna`. | Promote `TrgOps`/`OptFlds` into an explicit subscription profile contract with source tracking: SCD default, UnitLab default, live override, and operator-approved advanced override. |
| Time and quality semantics | Quality and source timestamp are surfaced when present. | Normalize IEC quality bits, timestamp precision/invalidity/leap-second flags, and separate source timestamp from backend receive timestamp. |
| Multi-device support | Current flow is one active debug session/device. | Add multiple concurrent associations, per-device runtime state, per-device event queues, isolation of report streams, and reconnect/resubscribe behavior. |
| Runtime eventing | Manual IED report values are pushed by the backend on `external-ieds/manual-reports` when the lease-owned backend reader observes changed report values; REST heartbeat only renews the manual lease. | Extend the same backend-published delta pattern to production verification subscriptions, retaining REST snapshot endpoints for initial load and reconnect recovery. |
| Diagnostics | Command failure codes are structured enough for current slices. | Add protocol-level diagnostics for APDU decode failures, service errors, access-result details, reject/error PDUs, timeout phase, invoke-id correlation, and pcap-friendly frame identifiers. |
| Conformance tests | Focused service tests and live pcap checks exist. | Add replay tests from captured MMS frames, simulator/client interoperability matrix, negative APDU tests, and cross-checks against an external reference client. |

### Server / simulator gaps before IEDScout-class parity

| Area | Current state | Gap to close |
| --- | --- | --- |
| Association lifecycle | Server accepts the current native client flow. | Implement full association release/abort behavior, negotiated limits, multiple simultaneous clients, and deterministic cleanup of reserved/enabled RCBs. |
| MMS service coverage | Server supports the services needed by current discovery/read/write/report flow. | Complete generic confirmed Read/Write/GetNameList handling for all supported model object classes and return standards-shaped service errors for unsupported operations. |
| Model exposure | SCD-derived model is exposed enough for current reports. | Expose a complete IEC 61850 object namespace: logical devices, logical nodes, data objects, data attributes, FC partitions, data sets, RCBs, and type-consistent values. |
| Response pagination | Large model responses can exceed simple frame assumptions. | Implement consistent `moreFollows`/continuation support for large GetNameList responses and validate client behavior against it. |
| Data values | Current simulator can emit selected structured values and primary values. | Add complete BER encoding for supported IEC 61850 primitive and constructed types, arrays, nested structures, qualities, timestamps, enum/int/float/string variants, and stable type metadata. |
| Write validation | Selected RCB writes and selected data writes are handled. | Enforce write permissions, FC constraints, type validation, RCB disabled-state constraints, per-attribute access errors, and safe rejection semantics. |
| RCB behavior | `RptEna`, `GI`, `OptFlds`, `TrgOps`, and basic reports work. | Complete BRCB/URCB state machines: reservation/owner, buffer queue, purge buffer, entry-id resume, sequence rollover, buffer overflow, integrity reporting, dataset mismatch behavior, and multiple clients competing for instances. |
| Report generation | GI and simple change reports are emitted. | Emit standards-shaped reports for every optional-field combination, inclusion bitstring subset, partial data-set updates, multi-member changes, integrity cycles, buffered replay, and overflow/recovery. |
| Error/reject behavior | Failed writes are surfaced enough for current client validation. | Add full confirmed-service error responses, reject PDUs for malformed APDUs, access-result errors per item, and deterministic diagnostics for unsupported features. |
| Timing behavior | Current timing is debug-oriented. | Add configurable report buffering time, integrity period, debounce/coalescing behavior, association idle handling, and deterministic test clocks. |
| Client interoperability | Validated primarily against the UnitLab native client and selected IEDScout captures. | Validate server behavior with IEDScout and at least one additional reference client for discovery, read, write, RCB enable, GI, and data-change reporting. |

### Recommended completion order

1. Freeze the single-device report runtime contract: report metadata, signal-state projection, current-state merge, and diagnostics.
2. Add pcap replay tests for GI and data-change reports, including reason-code and inclusion-bitstring coverage.
3. Complete RCB lifecycle behavior for BRCB and URCB before broadening into unrelated MMS services.
4. Harden discovery pagination and typed model building so connect-from-discovery does not depend on SCD shortcuts.
5. Move subscription options into an explicit profile contract and expose effective options read-only in the UI.
6. Extend the manual-inspector WebSocket delta path to production verification subscriptions after the event payload is stable.
7. Expand server conformance only after the client can consume the same behavior from an external IED or reference simulator.

## Ownership Model

- C owns protocol primitives and the wire-level MMS engine.
- Python owns orchestration, diagnostics, evidence, APIs, tests, and process management around the engine.
- Vue/TS owns the engineering workspace.
- The backend runtime and simulator both target the same UnitLab-owned service contracts so the transport engine can be swapped without changing report-flow semantics. The first live-wire FAT step uses a separate compose-service virtual IED with data/control sockets so Wireshark can observe the real on-network dialogue.

## Decision

Build the IEC 61850 report runtime around a stable UnitLab adapter boundary, but keep the transport/runtime implementation owned by UnitLab:

```text
SCD + Signal List
  -> report subscription plan
  -> UnitLab IEC 61850 runtime
  -> UnitLab MMS client / UnitLab MMS simulator
  -> real IED or virtual IED
```

`libiec61850` is a reference implementation and interoperability oracle, not the runtime authority. It may be used to compare behavior, inspect frames, and validate parity, but UnitLab code must own the client/server state machines, diagnostics, and simulator behavior.

The self-owned MMS client and the simulator should follow the same internal service flow and share the same report-control lifecycle model so the virtual IED and the real IED execute the same UnitLab semantics. The client-side boundary is mandatory because UnitLab will subscribe to its own virtual IEDs and ingest their report streams as evidence.

## Runtime Architecture

Target architecture for the self-owned MMS path:

```text
endpoint identity (IP / hostname / port)
  -> backend session orchestration
  -> native MMS client session
  -> discovery / browse / read / write / report-control state machine
  -> report decoder
  -> backend runtime snapshot
  -> UI / report consumers
```

### CLI entrypoint boundary

- `src/app/main.c` is now a thin dispatcher only.
- Public CLI API is only `src/app/cli/unitlab_cli.h` with `unitlab_cli_run(...)`.
- Internal CLI state lives in `src/app/cli/unitlab_cli_options.h` and `src/app/cli/unitlab_cli_internal.h`.
- Command routing lives in `src/app/cli/unitlab_cli_dispatch.c`.
- Command execution is split across `src/app/cli/commands/unitlab_cmd_fixture.c`, `src/app/cli/commands/unitlab_cmd_scl.c`, and `src/app/cli/commands/unitlab_cmd_mms_client.c`.
- `src/app/cli/unitlab_cli_common.c` holds shared internal utilities only.

Ownership boundaries:

- Backend owns endpoint resolution, session lifecycle, retry/recovery policy, evidence persistence, and the runtime snapshot contract.
- Native MMS client owns ACSE association, confirmed requests, browse/read/write/report-control commands, and report reception.
- The simulator and the real IED must exercise the same UnitLab service contract once they enter the MMS boundary.
- `libiec61850` remains the behavior oracle for pcap comparison and parity checks, not the production authority.

Discovery and subscription flow:

- Discovery starts from host/port when no richer model is available; SCD is optional input, not a hard prerequisite.
- The first browse step should establish the live VMD/domain context, then enumerate logical devices, logical nodes, data objects, data attributes, named variable lists, data sets, and report controls.
- A single bad leaf or unsupported object class must be skipped with diagnostics instead of aborting the whole browse session.
- One failed live domain browse must not abort the full root discovery if other logical-device domains are still available.
- `RptEna` and `GI` are explicit state-machine actions, not UI side effects.
- Report decoding must merge into the current backend signal-state projection so report arrivals update the runtime reactively.

Parity target versus the lib baseline:

- Match the successful service mix observed in the library-backed baseline: `GetNameList`, `GetVariableAccessAttributes`, `GetNamedVariableListAttributes`, `Read`, `RptEna`, `GI`, and report drain.
- Preserve the same logical-device / logical-node / dataset / report-control coverage across the same endpoint.
- Keep the debug/runtime contract honest: if discovery or GI is partial, the snapshot must say so rather than synthesizing a fully healthy state.

Current implementation posture:

- Native discover is no longer expected to be coupled to a hidden library fallback.
- The first release still supports report-oriented browsing and subscription, but it is not yet a full MMS model explorer across every LD/LN/DO/DA combination.
- The UI should remain report-centric until the runtime contract for full model browsing and reactive report patching is stable.

## Non-Negotiable Boundaries

- `frontend/src/modules/scd-sld-core` stays framework-neutral and does not know about MMS frames, sockets, native libraries, Java, or backend clients.
- `frontend/src/modules/iec61850-report-core` keeps portable DTOs, planning logic, report state semantics, diagnostics, and simulator-only validation.
- Backend/runtime owns real device sessions, command ordering, timeouts, reconnects, and evidence persistence.
- The self-owned MMS client and simulator must live behind an adapter boundary and must not leak BER, ACSE, COTP, or MMS PDU details into UnitLab domain models.
- Open-source or commercial stacks may be used as reference implementations, but not as hidden authorities for UnitLab business logic.
- The same UnitLab MMS flow must be used for both the virtual IED path and the future real-device path; only the transport endpoint changes.
- GOOSE, Sampled Values, controls, file services, and IEC 62351 security are out of scope for the first self-owned MMS client milestone.

## Local IEC 61850 Documents

These are already present in the repository and are the first source of truth for IEC 61850 behavior:

| Document | Local path | Use |
| --- | --- | --- |
| IEC 61850-6:2024 | `/workspace/docs/STANDARTS/.IEC61850/IEC 61850-6-2024.pdf` | SCL, IED, Communication, DataSet, ReportControl extraction |
| IEC 61850-7-2:2020 | `/workspace/docs/STANDARTS/.IEC61850/IEC 61850-7-2-2020.pdf` | ACSI services, reports, DataSet, RCB attributes, GI, trigger options, optional fields |
| IEC 61850-8-1:2020 | `/workspace/docs/STANDARTS/.IEC61850/IEC 61850-8-1-2020.pdf` | MMS mapping for IEC 61850 client/server communication |
| IEC 61850-7-3:2020 | `/workspace/docs/STANDARTS/.IEC61850/IEC 61850-7-3-2020.pdf` | Common data classes and value shape interpretation |
| IEC 61850-7-4:2020 | `/workspace/docs/STANDARTS/.IEC61850/IEC 61850-7-4-2020.pdf` | Logical nodes and data object definitions |
| IEC TR 61850-7-5:2021 | `/workspace/docs/STANDARTS/.IEC61850/IEC TR 61850-7-5-2021.pdf` | Practical ACSI/report usage guidance |
| IEC TR 61850-7-500:2017 | `/workspace/docs/STANDARTS/.IEC61850/IEC TR 61850-7-500-2017.pdf` | Object modeling and usage examples |

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

### Slice 0 - UnitLab MMS Core Boundary

Before protocol work expands, define the internal boundary that both the client and simulator will share:

- `UnitLabMmsSession` owns association state, request correlation, timeout handling, and disconnect cleanup.
- `UnitLabMmsReportControl` owns `read / reserve / enable / disable / GI` semantics and report-control state transitions.
- `UnitLabMmsTransport` owns TCP/MMS framing and raw frame exchange only.
- `libiec61850` is used as a reference oracle for pcap comparison and behavioral parity tests, not as a production runtime dependency.
- The simulator reuses the same service-layer contract so the virtual IED and the future real IED exercise the same UnitLab state machine.

Exit criteria:

- The internal client/simulator API is stable enough that the backend report runtime can target it without knowing transport details.
- A single read-only request path and a single report-control state path exist in both the simulator and client design.

### Slice 0A - UnitLab MMS Semantic Contract

Before any BER, ACSE, or MMS encoder/decoder work lands, freeze the behavior contract for request success, failure, timeout, and report reception.

Deliverables:

- transport-independent semantic PDU structs;
- typed runtime event and diagnostic mapping;
- request lifecycle rules for pending requests;
- report acceptance semantics for `GI_PENDING -> REPORTING`;
- golden tests that prove wire-independent behavior.

Exit criteria:

- the wire layer can only decode into semantic results and diagnostics;
- runtime state transitions remain owned by UnitLab orchestration;
- no encoder or decoder changes lifecycle state directly.


Before protocol work expands, define the internal boundary that both the client and simulator will share:

- `UnitLabMmsSession` owns association state, request correlation, timeout handling, and disconnect cleanup.
- `UnitLabMmsReportControl` owns `read / reserve / enable / disable / GI` semantics and report-control state transitions.
- `UnitLabMmsTransport` owns TCP/MMS framing and raw frame exchange only.
- `libiec61850` is used as a reference oracle for pcap comparison and behavioral parity tests, not as a production runtime dependency.
- The simulator reuses the same service-layer contract so the virtual IED and the future real IED exercise the same UnitLab state machine.

Exit criteria:

- The internal client/simulator API is stable enough that the backend report runtime can target it without knowing transport details.
- A single read-only request path and a single report-control state path exist in both the simulator and client design.

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
- The backend subscription flow writes `OptFlds=06 7f 80` and `TrgOps=02 74` before `RptEna` so live MMS reports include data-change, quality-change, data-update, GI, sequence, timestamp, reason, dataset, data-reference, entry-id, config-revision, and buffer-overflow fields.
- Report reason-code decoding uses the same IEC 61850 bit-string mask as `TrgOps`: `0x40=data-change`, `0x20=quality-change`, `0x10=data-update`, `0x08=integrity`, `0x04=GI`, `0x02=application-trigger`.

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
- Native stdout contract for decoded report values: `native-wire-client: report-entry index=<n> reference=<mms-ref> dataRef=<mms-data-ref> value=<typed-summary> kind=<bool|int|uint|float|string|quality|timestamp|octets|unsupported> reason=<labels> datasetMatch=<true|false> discoveredMatch=<true|false>`.
- Backend UI contract keeps raw report leaf values in `ui_state.report.values` and projects them into operator-facing `ui_state.report.signal_states` keyed by DataSet member reference. The first projection rules select `$stVal` or `$general` as the primary value, `$q` as quality, and `$t` as source timestamp.
- `ui_state.report.signal_states` is a merged last-known-state projection. GI can initialize all known leaves; later subset `InformationReport` events patch only the reported leaves while preserving previous quality/timestamp/current values for other leaves. `ui_state.report.values` remains the raw leaf list for the latest report event.
- `Connect` and `Discover` are separate operations. `Connect` opens the persistent MMS association using the SCD/in-memory model when one is supplied, or in browse-only mode from endpoint host/port when no SCD is supplied. `connect-ied` no longer requires a prior `discover`. `Discover` remains an explicit advanced live model verification step. `RptEna` and `GI` can run from the loaded SCD model or from the live-discovered RCB once selected.
- The persistent native MMS client does not send an implicit startup `Read`. After association it emits `ready` and waits for explicit stdin commands (`discover`, direct RCB writes, or an opt-in startup read configured by CLI flags). This keeps IEDScout and real-device discovery from being aborted by a fixture-specific legacy read.
- Native client receive buffers cover the full TPKT length range for persistent client operations. Live `GetNameList` responses from IEDScout and real devices can exceed small simulator-sized frames, especially when `moreFollows=true` and many domain variables are returned.
- Native discovery does not read whole ReportControl structures. It discovers BRCB objects from the domain variable list and validates attribute access through concrete RCB attributes such as `RptEna`; report-control operations continue to use explicit attribute reads/writes.
- When SCD is present, backend writes RCB attributes by explicit MMS object path derived from the selected candidate: `<IED><LD> / <LN>$BR|RP$<ReportControl>$RptEna` and `<LN>$BR|RP$<ReportControl>$GI`. It must not use the native `rptena`/`gi` discovered-index commands unless live discovery selected an RCB.

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

- Implemented today: SCD report inventory, Signal List merge, subscription plan builder, simulator-only report runtime, normalized report event DTOs, observation mapping, backend simulator parity, incoming report routing, activation precheck gates, MMS endpoint catalog, external IED simulator fixture export, external IED simulator process scaffold, fixture parser/model materialization, report option records, external simulator model plan, fail-closed loader boundary, external simulator model-plan blueprint validation, backend external simulator process preparation, backend external simulator lifecycle guardrails, backend external simulator process-plan orchestration, backend external simulator endpoint resolution, backend external simulator run orchestration, native external simulator loader validation, DataSet member path blueprinting, initial-value type preservation, report-control runtime blueprinting, report-control bit-mask blueprinting, DataSetEntry variable blueprinting, linked IED/LD/LN container creation, native MMS association/discovery/RptEna/GI path, decoded `InformationReport` entries, and backend mapping from native report entries into UI report values.
- Not implemented today: production-ready multi-device MMS client, report persistence, GOOSE, Sampled Values, and IEC 62351 security.
- Implemented boundary step: backend has an explicit MMS endpoint catalog, a fail-closed unavailable MMS adapter, a JSON fixture boundary for an external IED simulator, a C/CMake simulator process scaffold, materialized fixture records, report option records, an external simulator model plan, a fail-closed loader boundary, validated model-plan blueprint records for future libIEC61850 construction, native loader guardrail tests, explicit DataSet member kind/DO/DA path records, typed initial values, ReportControl runtime fields and libIEC61850-compatible bit masks, DataSetEntry variable names, linked dynamic `IedModel`/LD/LN container creation, and a backend process-boundary helper for fixture writing, command construction, endpoint shape, endpoint resolution, dry-run startup checks, process-plan orchestration, run orchestration through the report-runtime boundary, no-shell spawn, premature-exit diagnostics, and terminate/kill cleanup.
- Next architecture step: define the shared UnitLab MMS core boundary, then implement the client and simulator on top of it and validate parity against libiec61850.
