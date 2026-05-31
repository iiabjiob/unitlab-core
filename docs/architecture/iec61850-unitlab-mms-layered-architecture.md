# UnitLab MMS Layered Architecture

Status: target architecture. This document defines the intended layer split for the UnitLab-owned MMS stack before the wire-level implementation starts. The first transport-independent `semantic/` layer stub now exists in `simulator/iec61850_ied/src/unitlab_mms_semantic_pdu.{h,c}`.

See also:
- [UnitLab MMS Semantic Contract](./iec61850-unitlab-mms-semantic-contract.md)
- [UnitLab IEC 61850 MMS Core Boundary](./iec61850-unitlab-mms-core-boundary.md)
- [IEC 61850 Self-Owned MMS Client Plan](./iec61850-self-owned-mms-client-plan.md)

## Goal

Keep wire-level encoding/decoding isolated from runtime ownership and IEC 61850 report semantics.

The main rule is simple:

- wire code only converts bytes to semantic PDUs and back;
- runtime code owns lifecycle decisions;
- IEC 61850 report runtime owns RCB/report behavior;
- UnitLab orchestration owns FAT/evidence workflow.

## Target Module Layout

```text
native/unitlab_mms/
  include/
    unitlab_mms_core.h
    unitlab_mms_semantic_pdu.h
    unitlab_mms_diagnostics.h
    unitlab_mms_runtime_event.h

    unitlab_mms_ber.h
    unitlab_mms_tpkt.h
    unitlab_mms_cotp.h
    unitlab_mms_acse.h
    unitlab_mms_presentation.h
    unitlab_mms_pdu.h
    unitlab_mms_codec.h
    unitlab_mms_socket_transport.h

    unitlab_iec61850_report_runtime.h
    unitlab_iec61850_report_mapping.h

  src/
    core/
      unitlab_mms_diagnostics.c
      unitlab_mms_runtime_event.c
      unitlab_mms_pending_request.c
      unitlab_mms_session.c
      unitlab_mms_transport_exchange.c

    semantic/
      unitlab_mms_semantic_pdu.c
      unitlab_mms_semantic_result.c

    wire/
      ber/
        unitlab_mms_ber_reader.c
        unitlab_mms_ber_writer.c
        unitlab_mms_ber_tag.c
        unitlab_mms_ber_length.c

      iso/
        unitlab_mms_tpkt.c
        unitlab_mms_cotp.c

      acse/
        unitlab_mms_acse_encode.c
        unitlab_mms_acse_decode.c

      presentation/
        unitlab_mms_presentation_encode.c
        unitlab_mms_presentation_decode.c

      mms/
        unitlab_mms_pdu_encode.c
        unitlab_mms_pdu_decode.c
        unitlab_mms_read_service.c
        unitlab_mms_write_service.c
        unitlab_mms_information_report.c

      transport/
        unitlab_mms_socket_transport.c
        unitlab_mms_scripted_transport.c
        unitlab_mms_recorded_transport.c

    iec61850/
      unitlab_iec61850_report_runtime.c
      unitlab_iec61850_report_mapping.c
      unitlab_iec61850_rcb_paths.c
```

Notes:

- This layout is a target boundary, not an implemented directory contract yet.
- `client/` and `server/` directories are intentionally deferred until the lower layers are stable.
- `scripted` and `recorded` transports stay under the transport boundary for parity tests and replay.

## Layer Ownership

| Layer | Owns | Does not own |
| --- | --- | --- |
| core | diagnostics, runtime events, pending requests, sessions, transport exchange | BER, ACSE, MMS PDU decoding, IEC 61850 report semantics |
| semantic | transport-independent PDU structs and semantic results | sockets, framing, runtime lifecycle decisions |
| wire | bytes <-> semantic PDU conversion | runtime state changes, FAT workflow, report-control semantics |
| iec61850 | RCB lifecycle, GI, report acceptance, report mapping | BER, socket lifecycle, general MMS transport |
| client | high-level client flow (future) | wire parsing internals |
| server | high-level server flow (future) | wire parsing internals |

The semantic layer is the bridge between runtime ownership and wire-level codec work. It projects decoded transport data into `UnitLabMmsSemanticResult` and `UnitLabMmsDecodedPdu` before any runtime mutation happens.

## Non-Negotiable Boundaries

- `wire/` must not know about UnitLab FAT workflow.
- `wire/` must not call runtime state transitions directly.
- `wire/` must not mutate session or report-control state.
- `semantic/` must stay transport-independent.
- `iec61850/` must not own BER or socket framing.
- `core/` remains the runtime owner for request correlation, timeouts, and diagnostics.

## First Wire Slice

The first wire slice is intentionally small:

- Associate
- Release
- Read one variable
- Write one variable
- Receive one InformationReport

Out of scope for the first wire slice:

- generic MMS object model
- generic ASN.1 framework
- dynamic schema engine
- client/server feature expansion beyond the first slice

## Evolution Path

1. Freeze semantic contract.
2. Land runtime ownership and semantic PDU structs.
3. Add transport-independent golden tests.
4. Implement TPKT/COTP and minimal BER.
5. Implement ACSE associate/release/abort.
6. Implement minimal MMS confirmed request/response.
7. Add report mapping and incoming report routing.

## Risk Notes

- If decode code starts mutating runtime state, the architecture has drifted.
- If BER code grows into a generic ASN.1 framework before the first slice is complete, the implementation scope has expanded too far.
- If IEC 61850 report semantics are mixed into wire code, the client/server split will become hard to maintain.
