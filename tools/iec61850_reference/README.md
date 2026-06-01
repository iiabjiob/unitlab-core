# IEC 61850 Reference Capture Workflow

This directory is a dev-only comparison harness.

Use it to generate known-good IEC 61850/MMS traffic with an external libiec61850 checkout, capture it in Wireshark, and compare the packet shape against the UnitLab native virtual IED.

What this is:
- a reference capture workflow for protocol comparison;
- an external, dev-only use of libiec61850;
- a way to validate the expected association and MMS sequence before changing UnitLab code.

What this is not:
- production UnitLab runtime code;
- a libiec61850 vendoring path;
- a substitute for the UnitLab native MMS implementation.

Licensing boundary:
- do not copy libiec61850 source into UnitLab;
- do not port libiec61850 encoders or tables into UnitLab;
- keep the reference checkout outside the UnitLab runtime tree.

## External checkout

Clone libiec61850 outside the UnitLab implementation path, for example:

```bash
mkdir -p /workspace/external
git clone https://github.com/mz-automation/libiec61850.git /workspace/external/libiec61850
```

The UnitLab repository ignores `external/libiec61850/` and the local capture directory under this workflow.

## Environment

These scripts accept the following environment variables:

- `LIBIEC61850_ROOT`: path to the external libiec61850 checkout
- `LIBIEC61850_SERVER_BIN`: absolute path to the reference server binary
- `LIBIEC61850_CLIENT_BIN`: absolute path to the reference client binary
- `LIBIEC61850_SERVER_ARGS`: extra arguments passed to the reference server
- `LIBIEC61850_CLIENT_ARGS`: extra arguments passed to the reference client
- `CAPTURE_INTERFACE`: capture interface for `tcpdump`
- `CAPTURE_PCAP`: output pcap path
- `CAPTURE_PORT`: TCP port to capture, defaults to `12449`

If your libiec61850 example binaries use different names, point the variables at the executables directly.

Typical lookup commands for a built external checkout:

```bash
export LIBIEC61850_SERVER_BIN="$(find /workspace/external/libiec61850 -type f -perm -111 -name 'server_example_basic_io' | head -n 1)"
export LIBIEC61850_CLIENT_BIN="$(find /workspace/external/libiec61850 -type f -perm -111 -name 'iec61850_client_example1' | head -n 1)"
```

If the reference example you are using is hard-coded to the standard MMS port, keep that change outside the UnitLab tree and expose it on `12449` from the reference checkout or wrapper. This workflow intentionally keeps UnitLab code untouched.

## Preferred port layout

- Reference libiec61850 server: `12449`
- UnitLab native virtual IED: `12447`

Do not use TCP port `102` in this workflow unless you are testing a separate legacy path.

## Build and run the reference stack

If your external libiec61850 checkout already contains built example binaries, set the binary paths explicitly:

```bash
export LIBIEC61850_SERVER_BIN=/workspace/external/libiec61850/examples/server_example_basic_io/server_example_basic_io
export LIBIEC61850_CLIENT_BIN=/workspace/external/libiec61850/examples/client_example_basic_io/client_example_basic_io
```

If your local build places the binaries elsewhere, use those paths instead.

Start the server:

```bash
tools/iec61850_reference/run_reference_server.sh
```

Start the client in a second terminal:

```bash
tools/iec61850_reference/run_reference_client.sh
```

If your sample binary accepts a port argument, pass it through `LIBIEC61850_SERVER_ARGS` and `LIBIEC61850_CLIENT_ARGS` so the reference server listens on `12449`.

If your example binary does not accept a port argument, adapt the external checkout or wrap it outside UnitLab so the listener is reachable on `12449`.

## Capture workflow

Start capture before you run the client:

```bash
tools/iec61850_reference/capture_reference.sh
```

Default output:

```text
tools/iec61850_reference/captures/libiec61850-reference.pcap
```

Open the pcap in Wireshark and decode TCP port `12449` as TPKT if Wireshark does not pick it up automatically:

- `Analyze -> Decode As...`
- TCP port `12449` -> `TPKT` / ISO on TCP

Useful filter:

```text
tcp.port == 12449
```

## Expected packet sequence

Reference association sequence:

1. TCP handshake
2. COTP CR from client
3. COTP CC from server
4. Session connect / accept
5. Presentation CP / CPA
6. ACSE AARQ from client
7. ACSE AARE from server
8. MMS InitiateRequest
9. MMS InitiateResponse

Basic MMS traffic:

1. MMS Confirmed-RequestPDU
2. MMS Confirmed-ResponsePDU

## Compare against UnitLab native wire server

UnitLab native server:
- port: `12447`

Reference server:
- port: `12449`

Compare these points first:

- first server response after TCP connect;
- whether COTP CC appears;
- whether AARE appears in a proper association accept context;
- whether MMS InitiateResponse is decoded;
- whether UnitLab is still sending AARE inside ordinary Session DATA TRANSFER;
- where the first byte-level divergence starts.

## Comparison checklist

Use the same ordering for both captures. Stop at the first layer that diverges.

1. TCP handshake
2. COTP CR from client
3. COTP CC from server
4. Session connect / accept
5. Presentation CP / CPA
6. ACSE AARQ from client
7. ACSE AARE from server
8. MMS InitiateRequest
9. MMS InitiateResponse
10. First confirmed MMS request/response pair
11. First report-control exchange, if present

Per packet notes to record:
- source/destination port
- first 32 payload bytes
- first layer Wireshark decodes cleanly
- first layer Wireshark stops at
- whether the payload is on the association path or the operational MMS path

## Analysis template

Use `analysis-template.md` to record a side-by-side comparison between the reference pcap and the UnitLab pcap.

