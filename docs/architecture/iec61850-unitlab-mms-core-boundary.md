# UnitLab IEC 61850 MMS Core Boundary

Status: architecture note and implementation boundary. This document defines the UnitLab-owned MMS runtime layer that will back both the virtual IED simulator and the future real-device client.

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

The same internal flow must be used for both the simulator and the future real IED path. Only the transport endpoint changes.

### Owned by UnitLab

- Session lifecycle and association state.
- Request correlation and timeout handling.
- Report-control lifecycle: read, reserve, enable, disable, GI, release.
- MMS transport framing and request/response dispatch.
- Diagnostics and evidence DTOs.
- Simulator behavior and failure injection.

### Reference-only

- `libiec61850` for pcap comparison and parity validation.
- Wireshark dissectors for protocol inspection.
- Existing external simulator behavior as a conformance target.

## Layers

### 1. Transport

Own TCP, TPKT, COTP, presentation, ACSE, and MMS framing in a dedicated transport layer.

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

## Contract Principles

- Domain code must not see BER, ACSE, COTP, or MMS PDU details.
- Runtime code must consume normalized report dataset leaves, not raw SCL dataset members.
- The simulator and client must share the same lifecycle semantics.
- `libiec61850` may validate behavior, but it must not own the contract.

## Current Implementation

- The backend now exposes the UnitLab MMS boundary contracts in `backend/app/services/iec61850/unitlab_mms_core.py`.
- The in-memory simulator already conforms to `UnitLabMmsSession` and `UnitLabMmsRuntimeAdapter` via runtime-checkable protocols.
- Deterministic scripted and recorded transport helpers now exist for transport-oriented tests and parity capture, but they are not wire MMS framing.
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

### Slice 5 - Simulator Parity

Apply the same contract to the virtual IED simulator and validate against `libiec61850` as a reference oracle.

Exit criteria:

- simulator and client follow the same UnitLab state machine;
- parity tests cover association, RCB access, GI, and cleanup.

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
