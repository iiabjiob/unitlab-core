# UnitLab IEC 61850 MMS Core Boundary

Status: architecture note and implementation boundary. This document defines the UnitLab-owned MMS runtime layer that will back both the virtual IED simulator and the future real-device client.

See also: [UnitLab MMS Semantic Contract](./iec61850-unitlab-mms-semantic-contract.md).
See also: [UnitLab MMS Layered Architecture](./iec61850-unitlab-mms-layered-architecture.md).
See also: [UnitLab IEC 61850 MMS Client Boundary](./iec61850-unitlab-mms-client-boundary.md).

## Goal

Build a UnitLab-owned IEC 61850 MMS core that controls association, request correlation, report-control state, and report activation semantics without depending on `libiec61850` as a runtime authority.

`libiec61850` remains a reference implementation for parity checks, packet comparison, and behavior review. It must not define the product domain model or own the runtime state machine.

## Boundary

```text
SCD + Signal List
  -> report subscription plan
  -> UnitLab IEC 61850 runtime
  -> UnitLab MMS client / UnitLab MMS simulator
  -> virtual IED or real IED
```

The same internal flow must be used for both the simulator and the future real IED path. Only the transport endpoint changes. The backend client-side façade is now present and reuses the shared report-runtime contract; the wire transport remains a later replacement boundary.

## Ownership Model

The implementation stack is intentionally split by responsibility:

- C owns the wire-level protocol engine:
  - protocol primitives;
  - BER;
  - MMS PDUs;
  - transport and session framing;
  - parser and encoder;
  - embedded portability.
- Python owns orchestration around that engine:
  - backend runtime;
  - diagnostics and evidence;
  - APIs;
  - tests;
  - process management;
  - simulator/session control;
  - runtime event/result shaping.
- Vue/TS owns the engineering workspace and operator-facing UI.

The UnitLab MMS boundary in this document is the seam where the C implementation will plug in later. The Python-side contracts stay stable so the runtime, simulator, and evidence flow do not need to change when the transport engine is replaced.

### Owned by UnitLab

- Session lifecycle and association state.
- Request correlation and timeout handling.
- IEC 61850 report-control semantics: read, reserve, enable, disable, GI, release.
- Transport framing and request/response dispatch.
- Diagnostics, evidence DTOs, and runtime event/result structs.
- Simulator behavior and failure injection.

### Reference-only

- `libiec61850` for pcap comparison and parity validation.
- Wireshark dissectors for protocol inspection.
- Existing external simulator behavior as a conformance target.

## Layers

### 1. Transport

Own TCP, TPKT, COTP, presentation, ACSE, and MMS framing in a dedicated transport layer. The long-term implementation target for this layer is C.

Responsibilities:

- open and close the socket;
- frame and parse bytes;
- carry invoke IDs and responses;
- report protocol-level errors explicitly.

### 2. Session And Association

Own association setup, release, and abort handling.

Responsibilities:

- connect/disconnect;
- association negotiation;
- association cleanup on failure;
- state transitions that back the runtime lifecycle.

### 3. Report-Control Service Layer

Own the IEC 61850 report-control operations.

Responsibilities:

- read live RCB metadata;
- reserve/release where supported;
- enable/disable reporting;
- GI triggering;
- decode `InformationReport` into UnitLab DTOs.

### 4. Simulator Backing

Use the same service-layer contract to drive the virtual IED.

Responsibilities:

- deterministic state transitions;
- injected errors and cleanup behavior;
- parity with the client-side service flow.

### 5. Client Orchestration

Use the same service-layer contract to consume report streams from the virtual IED or a real IED.

Responsibilities:

- subscribe to report streams;
- trigger GI and correlate responses;
- record report evidence and runtime snapshots;
- keep the same lifecycle semantics as the server/runtime side.

## Contract Principles

- Domain code must not see BER, ACSE, COTP, or MMS PDU details.
- Runtime code must consume normalized report dataset leaves, not raw SCL dataset members.
- The simulator and client must share the same lifecycle semantics.
- `libiec61850` may validate behavior, but it must not own the contract.

## Current Implementation

- The backend now exposes the UnitLab MMS boundary contracts in `backend/app/services/iec61850/unitlab_mms_core.py` plus the dedicated IEC 61850 report runtime layer in `backend/app/services/iec61850/unitlab_iec61850_report_runtime.py`. The MMS core owns session, invoke IDs, pending requests, transport exchange, diagnostics, runtime events, and runtime snapshots. The report runtime layer owns RCB lifecycle, GI semantics, and report acceptance transitions (`GI_PENDING -> REPORTING`).
- The in-memory simulator already conforms to `UnitLabMmsSession` and `UnitLabMmsRuntimeAdapter` via runtime-checkable protocols.
- Deterministic scripted and recorded transport helpers now exist for transport-oriented tests and parity capture, a transport exchange helper binds request/response bytes with bounds checks, an in-memory association helper exercises open/release/abort semantics, a dedicated IEC 61850 report-control runtime helper covers reserve/enable/GI/accept-report/disable/release transitions, a named-variable access helper covers read/write/snapshot behavior, the MMS kernel now exposes typed runtime events, request-correlation and timeout DTOs, transport-independent semantic PDU structs, and a copy-safe runtime snapshot DTO that captures report-control state and trace fields separately from the core transport/session state. The MMS core does not implement wire MMS framing.
- Real MMS wire transport remains a planned layer.

## Implementation Slices

### Slice 0 - Core Boundary

Define the shared internal interfaces for transport, association, report-control state, and diagnostics.

Exit criteria:

- client and simulator can compile against the same service contract;
- no runtime code depends on `libiec61850` types.

### Slice 1 - Transport

Implement raw frame exchange and protocol error reporting.

Exit criteria:

- bytes can be sent and received deterministically;
- disconnect and timeout paths are typed.

### Slice 2 - Association

Implement association, release, and abort.

Exit criteria:

- the runtime can connect and disconnect from a test endpoint;
- failed association produces explicit diagnostics.

### Slice 3 - Named Variable Access

Implement the minimum read/write primitives needed for report-control metadata.

Exit criteria:

- live RCB state can be read from a test endpoint;
- write attempts are gated by state and diagnostics.

### Slice 4 - Report Control And GI

Implement reserve/enable/disable/GI and incoming report normalization.

Exit criteria:

- a selected report-control path can be exercised end to end;
- report events decode into UnitLab evidence DTOs.

### Slice 5 - Client Parity

Apply the same contract to the client orchestration path and validate against `libiec61850` as a reference oracle.

Exit criteria:

- client and simulator follow the same UnitLab state machine;
- parity tests cover association, RCB access, GI, report reception, and cleanup.

## Validation Expectations

- Unit tests for state transitions, request correlation, and diagnostics.
- Golden pcap comparison against `libiec61850` or a known-good MMS endpoint.
- Simulator-only parity tests before any real IED connection is enabled.
- Real IED smoke tests only after the client path is stable and isolated.

## Risks

- Vendor-specific MMS behavior may require adapter-specific diagnostics.
- Transport bugs can look like IED failures if the error boundary is too weak.
- The client and simulator must not diverge in lifecycle semantics.
- Real-device support must remain behind explicit backend ownership.
