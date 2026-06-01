# UnitLab IEC 61850 MMS Client Boundary

Status: architecture note and implementation boundary. This document defines the UnitLab-owned MMS client/orchestration layer that will consume reports from the virtual IED first and later from a real IED.

See also: [UnitLab IEC 61850 MMS Core Boundary](./iec61850-unitlab-mms-core-boundary.md).
See also: [UnitLab MMS Semantic Contract](./iec61850-unitlab-mms-semantic-contract.md).
See also: [UnitLab MMS Layered Architecture](./iec61850-unitlab-mms-layered-architecture.md).
See also: [IEC 61850 Self-Owned MMS Client Plan](./iec61850-self-owned-mms-client-plan.md).

## Goal

Build a UnitLab-owned IEC 61850 MMS client/orchestration layer that subscribes to reports, drives GI, correlates responses, and records evidence without depending on `libiec61850` as a runtime authority.

The client must use the same UnitLab protocol contracts as the virtual IED/server path. The only difference between the simulator and real-device path is the transport endpoint.

## Boundary

```text
SCD + Signal List
  -> report subscription plan
  -> UnitLab IEC 61850 client runtime
  -> UnitLab MMS client / UnitLab MMS simulator
  -> virtual IED or real IED
```

The client consumes the same report-control and semantic contracts that the server-side runtime uses. The client does not own SCL parsing, and it does not guess protocol semantics from raw wire data.

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

The UnitLab MMS client boundary is the seam where the orchestration layer will consume the lower protocol stack later. The orchestration contracts must remain stable so client runtime, evidence, and simulator behavior can evolve without changing the semantic model.

### Owned by UnitLab

- Association setup, release, and abort handling on the client side.
- Request correlation, timeout handling, and resend policy.
- Report subscription activation and GI trigger flow.
- Incoming report normalization and evidence capture.
- Runtime event/result snapshots and client-side transcripts for replay and diagnostics.
- Simulator-backed parity behavior.

### Reference-only

- `libiec61850` for pcap comparison and parity validation.
- Wireshark dissectors for packet inspection.
- Existing external IED behavior as a conformance target.

## Layers

### 1. Transport

Own socket/TCP connectivity and protocol framing.

Responsibilities:

- open and close the socket;
- frame and parse bytes;
- report protocol-level errors explicitly;
- preserve consumed length for stream parsing.

### 2. Association And Session

Own association setup and teardown.

Responsibilities:

- connect/disconnect;
- association negotiation;
- release and abort;
- state transitions that back the runtime lifecycle.

### 3. Subscription And Report Flow

Own the client-side report-control workflow.

Responsibilities:

- read live RCB metadata;
- reserve/enable/disable where required;
- trigger GI;
- consume `InformationReport` into UnitLab DTOs;
- correlate reports to the active subscription plan;
- record missing or malformed report evidence.

### 4. Simulator Backing

Use the same service-layer contract to drive the virtual IED path.

Responsibilities:

- deterministic state transitions;
- injected errors and cleanup behavior;
- parity with the client-side service flow.

## Contract Principles

- Domain code must not see BER, ACSE, COTP, or MMS PDU details.
- Client code must consume normalized report dataset leaves, not raw SCL dataset members.
- The client and server must share the same lifecycle semantics.
- `libiec61850` may validate behavior, but it must not own the contract.
- Client evidence must be replayable from event/result snapshots, not from UI state.

## Current Implementation

- The lower-level UnitLab MMS runtime boundary already exists in the simulator tree and the Python backend service layer; both expose session, pending request, transport exchange, diagnostics, runtime events, runtime snapshots, report-control ownership, and semantic bridges.
- The virtual IED/server path already uses the UnitLab-owned report runtime and MMS lower layer to process incoming bytes and build first-slice confirmed response frames.
- The backend client-side façade now exists in `backend/app/services/iec61850/client_runtime.py`, is exported through `backend/app/services/iec61850/__init__.py`, and reuses the same report-runtime contract for association, report-control operations, GI, and subscription-plan execution. It now also keeps a replayable transcript of client-side orchestration events for evidence/debug capture. A small backend control API in `backend/app/api/v1/iec61850/router.py` and `backend/app/services/iec61850/client_control.py` powers the first frontend MMS client test page at `frontend/src/pages/debug61850/Iec61850ClientTestPage.vue`. That control API now also has a live wire transport slice that can start the native TCP listener, trigger report emission, and capture the raw TPKT frame bytes for Wireshark-visible transport smoke. It is still transport-agnostic at the MMS semantic layer and does not parse BER, ACSE, COTP, or MMS PDUs directly.

## Implementation Slices

### Slice 0 - Client Boundary

Define the shared internal interfaces for client transport, association, subscription control, and diagnostics.

Exit criteria:

- client orchestration can compile against the same service contract as the server-side runtime;
- no runtime code depends on `libiec61850` types.

### Slice 1 - Association And Session

Implement client connect, release, and abort.

Exit criteria:

- the client can connect and disconnect from a test endpoint;
- failed association produces explicit diagnostics.

### Slice 2 - Subscription Activation

Implement read, reserve, enable, disable, and GI on the client side.

Exit criteria:

- a selected report-control path can be exercised end to end;
- enable, GI, disable, and release produce auditable client events.

### Slice 3 - Incoming Report Routing

Implement report reception, normalization, and evidence capture.

Exit criteria:

- one selected Signal List row can be proven through one real or simulated report;
- report observations are mapped back to the active subscription plan.

### Slice 4 - Simulator Parity

Apply the same contract to the virtual IED simulator and validate against `libiec61850` as a reference oracle.

Exit criteria:

- client and simulator follow the same UnitLab state machine;
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
