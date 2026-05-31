# UnitLab MMS Layered Architecture

Status: target architecture. This document defines the intended layer split for the UnitLab-owned MMS stack before the wire-level implementation starts. The first transport-independent `semantic/` layer stub exists in `simulator/iec61850_ied/src/unitlab_mms_semantic_pdu.{h,c}`, the initial wire foundation exists in `simulator/iec61850_ied/src/wire/{ber,iso,acse,presentation,mms}`, and the completed lower layer is packaged as the reusable static library `unitlab-iec61850-mms` with the umbrella header `unitlab_iec61850_mms.h`.

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

      session/
        unitlab_mms_session_spdu.c

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
- The reusable lower-layer C boundary is exposed through `unitlab-iec61850-mms` and `unitlab_iec61850_mms.h` for future server/client reuse.
- A thin server-side ownership aggregate is now being introduced on top of the reusable lower layer for future server orchestration.
- The server boundary now has explicit prepare/start/stop lifecycle state on top of the reusable lower layer.
- The server boundary also owns report-control reserve/enable/GI/release orchestration through the reusable report runtime layer.
- The server boundary now has a raw association-byte ingress helper that decodes the lower-layer wire chain before applying runtime ownership.

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

Initial implemented wire foundation:

- TPKT frame wrap/unwrap
- COTP CR/CC/DR/DT encode/decode with raw TPDU payload view
- BER tag encode/decode
- BER length encode/decode
- BER TLV read/write
- ACSE APDU top-level classify/wrap/unwrap using exact X.227 application tags for AARQ/AARE/RLRQ/RLRE/ABRT
- raw ACSE field view preserves the exact outer sequence elements without semantic interpretation
- session SPDU raw boundary that parses one X.225 SPDU via SI/LI and preserves the opaque parameter view
- presentation User-data boundary with exact X.226 simply-encoded-data and fully-encoded-data mapping
- RFC1006/TPKT + COTP transport frame composition helper
- association-fixture encode/decode helper across TPKT/COTP/exact Presentation User-data wrappers
- wire/orchestration confirmed-response frame builder that owns the nesting construction
- MMS MMSpdu top-level classify/wrap/unwrap
- narrow service classification for confirmed Read/Write responses and unconfirmed InformationReport choices

Presentation is intentionally exact-only at this boundary. The association fixture helper is decode-safe and encode-safe for staged Presentation User-data payloads, but it does not interpret ACSE or MMS semantics. The MMS PDU wrapper currently peels the top-level `invokeID` and the next service TLV for confirmed request/response/error PDUs, validates `invokeID` as a minimal Unsigned32 BER integer, and leaves full service field decode pending. The wire/orchestration builder owns confirmed-response nesting construction so server orchestration stays above the wire boundary.

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
