# UnitLab MMS Semantic Contract

Status: pre-wire contract. This document defines what counts as success, failure, timeout, and runtime transition before any BER, ACSE, or MMS encoder/decoder work starts.

See also:
- [UnitLab IEC 61850 MMS Core Boundary](./iec61850-unitlab-mms-core-boundary.md)
- [IEC 61850 Report Runtime Plan](./iec61850-report-runtime-plan.md)
- [IEC 61850 Self-Owned MMS Client Plan](./iec61850-self-owned-mms-client-plan.md)

## Purpose

The wire layer must not own runtime semantics. Its job is to decode wire data into semantic results and diagnostics. The runtime layer decides whether to:

- complete a request;
- fail a request;
- abort a session;
- mark a report as received.

The decoder may produce:

- `UnitLabMmsDecodedPdu`
- `UnitLabMmsDecodeDiagnostic`

but it must not directly mutate runtime lifecycle state.

## Boundary Rule

Wire layer cannot change lifecycle state directly.

It may only return semantic decode results and diagnostics. Runtime orchestration owns all state transitions.

## Layered Architecture Reference

The intended physical layer split is documented in [UnitLab MMS Layered Architecture](./iec61850-unitlab-mms-layered-architecture.md).

## Runtime Ownership Table

| Layer | Owns |
| --- | --- |
| Transport | bytes, framing, socket lifecycle |
| BER | tag/length/value parsing |
| MMS decode | semantic PDU extraction |
| Runtime | request lifecycle, state transitions |
| IEC 61850 runtime | RCB/report semantics |
| UnitLab orchestration | evidence/FAT workflow |

## Malformed Classification

The semantic layer must distinguish the following classes before any runtime transition is applied:

- decode failure
- semantic invalid
- unsupported semantic
- unexpected service
- correlation mismatch

These must not collapse into a generic protocol error unless the downstream runtime explicitly maps them that way for a documented reason.

## First Wire Slice Lock

The first wire slice is intentionally narrow.

In scope:

- Associate
- Release
- Read one variable
- Write one variable
- Receive one InformationReport

Out of scope for the first wire slice:

- generic MMS object model
- generic ASN.1 framework
- dynamic schema engine

## Core Semantic Flow

```text
semantic PDU structs
↓
state transition rules
↓
diagnostic mapping
↓
golden tests
↓
encoder / decoder
```

## Service Semantics Matrix

| Service | Request | Response | Reject | Timeout | Runtime event |
| --- | --- | --- | --- | --- | --- |
| Associate | yes | yes | yes | yes | `SESSION_ASSOCIATED` |
| Release | yes | yes | yes | yes | `SESSION_RELEASED` |
| Read RCB | yes | yes | yes | yes | `RCB_READ` |
| Write RptEna | yes | yes | yes | yes | `RCB_ENABLED` / `FAILED` |
| Write GI | yes | yes | yes | yes | `GI_REQUESTED` |
| InformationReport | no | incoming | malformed | n/a | `REPORT_RECEIVED` |
| Abort | optional | incoming | n/a | n/a | `SESSION_ABORTED` |

Notes:
- This matrix defines behavioral success and failure, not BER layout.
- `Reject` means the runtime must receive a typed negative outcome, not a silent failure.
- `Timeout` means the request lifecycle can expire before a semantic completion arrives.

## Semantic PDU Contract

The first transport-independent semantic PDU layer now exists in `simulator/iec61850_ied/src/unitlab_mms_semantic_pdu.{h,c}`. Before BER exists, the codebase must define transport-independent semantic structures for:

- association request/response/reject;
- read request/response/reject;
- write request/response/reject;
- information report incoming event;
- release and abort flows;
- report-control activation and GI flows;
- pending-request correlation and timeouts.

The first semantic PDU layer now separates service outcome from the payload shape:

- `UnitLabMmsSemanticResult` owns `ok`, `outcome`, `pdu`, and `diagnostic`.
- `UnitLabMmsDecodedPdu` owns transport-independent fields only.
- malformed/decode failure state lives in `UnitLabMmsDecodeDiagnostic`, not inside the PDU.
- reject-specific fields are reserved in `UnitLabMmsReject`.
- the first wire-to-semantic bridge maps first-slice MMS wire PDU classifications into these semantic results without mutating runtime state.
- the runtime bridge applies semantic results to session/request/report ownership, keeping wire decode and lifecycle transitions separate.
- first-slice confirmed READ/WRITE requests project into pending-request ingress, while responses remain the lifecycle-completing path.
- the server runtime may now build first-slice confirmed response bytes from a pending request through a wire/orchestration builder that owns the nesting construction and accepts caller-provided scratch without changing the wire/runtime split.
- server ingress is now split into incoming-bytes handling and a thin association-request validator, keeping the nested wire decode helper separate from server orchestration.

These structures are the only legal input to state transition rules.

## State Transition Rules

- A decoded semantic result may complete a pending request.
- A decoded semantic result may fail a pending request.
- A decoded semantic result may abort a session.
- A decoded semantic result may mark a report as received.
- A decoded semantic result may not directly alter transport ownership or bypass request correlation.

## Diagnostic Mapping

Diagnostics must remain domain-specific and actionable. At minimum, the pre-wire contract must distinguish:

- bad state;
- invoke-id mismatch;
- request already active;
- request not active;
- response not bound;
- buffer too small;
- RCB not reserved;
- RCB not enabled;
- timeout;
- protocol error;
- unsupported operation.

The mapping from semantic decode result to runtime diagnostic must be explicit and testable.

## Golden Tests

The first golden tests must validate:

- semantic decode output shape;
- runtime state transition behavior;
- diagnostic mapping;
- timeout handling;
- report acceptance behavior;
- reject handling;
- malformed incoming report handling.

These tests should run before the encoder/decoder slice lands so the wire layer cannot define semantics by accident.

## Implementation Order

1. Semantic PDU structs.
2. State transition rules.
3. Diagnostic mapping.
4. Golden tests.
5. BER / ACSE / MMS encoder and decoder.

## Non-Goals

- No BER codec implementation in this document.
- No ACSE implementation in this document.
- No MMS wire encoding in this document.
- No transport ownership changes in this document.
- No GOOSE or Sampled Values scope in this document.
