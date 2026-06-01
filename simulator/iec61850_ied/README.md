# UnitLab IEC 61850 IED Simulator

Status: internal test-tool scaffold. It validates the UnitLab fixture/model-plan/loader boundary, can start one libIEC61850 MMS server when built with libIEC61850, and can probe DataSet/BRCB metadata plus a GI report through a linked client.

This directory is the boundary for the future libIEC61850-based IED simulator. It is intentionally separate from UnitLab backend/core runtime so GPL/native code cannot leak into production logic by accident.

The C-owned seam starts in `src/unitlab_mms_core.c` and `src/unitlab_mms_core.h`. That module owns the UnitLab transport/session boundary plus the IEC 61850 report-control semantics that the future wire-level implementation will fill in. libIEC61850 stays in the simulator as a reference backend and interoperability oracle.

Current C-owned helpers include:

- session lifecycle and invoke-id correlation;
- IEC 61850 report-control reserve/enable/GI/disable/release transitions;
- runtime event/result records, short replay-oriented logs, and trace-carrying operation-result projection helpers;
- in-memory transport exchange helpers with request/response binding and association helpers used by focused tests.

## Purpose

- Consume the JSON fixture exported by backend IEC 61850 runtime.
- Expose one simulated IED over MMS for internal UnitLab tests when linked with libIEC61850.
- Let UnitLab backend connect to the simulator through the same `Iec61850ClientAdapter` contract used for real IEDs.
- Keep Signal List matching, FAT evidence, diagnostics, and report planning owned by UnitLab.

## Docker Dev Topology

Use the host-debug wire path for wire-debug runs:

- backend/client connects to `host.docker.internal:12447` and `host.docker.internal:12448` so the host can capture MMS/RFC1006/COTP/ACSE traffic in Wireshark;
- port `12447` is used instead of `102` to avoid privileged-port friction in the dev loop;
- do not use `localhost` from the dev container for the IED endpoint because it resolves to the backend container itself, not the virtual IED service;
- Wireshark capture filter: `tcp.port == 12447 || tcp.port == 12448`.

The `iec61850-ied` service in `docker-compose.dev.yml` starts the native wire server on port `12447` and uses `12447` and `12448` for host-side debugging.

## Capture Pcap

The devcontainer image includes `tcpdump`, so you can record a wire trace without installing anything after startup.

Start a capture in one terminal:

```bash
sudo tcpdump -i any -s 0 -w /workspace/iec61850-wire.pcap 'tcp port 12447 or tcp port 12448'
```

Then in another terminal run `Start wire` and `Emit wire report` in the debug page. The current native-wire smoke frame is a standards ACSE AARE server response, so Wireshark should decode the server-side association response through ACSE. The native data socket also accepts the follow-up confirmed read/write requests for the association path and returns standards-shaped confirmed responses. Stop the capture with `Ctrl+C` after the test.

Open `/workspace/iec61850-wire.pcap` in Wireshark on your host. If Wireshark does not decode the payload as MMS automatically, use `Analyze -> Decode As...` and map TCP ports `12447` and `12448` to the ISO-on-TCP / TPKT stack.

For a quick terminal check without writing a file, use:

```bash
sudo tcpdump -i any -nn -vv 'tcp port 12447 or tcp port 12448'
```

## Build

Dry-run scaffold build without libIEC61850:

```bash
cmake -S simulator/iec61850_ied -B /tmp/unitlab-iec61850-ied-build
cmake --build /tmp/unitlab-iec61850-ied-build
ctest --test-dir /tmp/unitlab-iec61850-ied-build --output-on-failure
```

Build with libIEC61850 availability check:

```bash
cmake -S simulator/iec61850_ied -B /tmp/unitlab-iec61850-ied-build \
  -DUNITLAB_IEC61850_SIM_WITH_LIBIEC61850=ON \
  -DLIBIEC61850_INCLUDE_DIR="/path/to/libiec61850/src/iec61850/inc;/path/to/libiec61850/src/common/inc;/path/to/libiec61850/hal/inc;/path/to/libiec61850/src/mms/inc;/path/to/libiec61850/src/logging" \
  -DLIBIEC61850_LIBRARY=/path/to/libiec61850/build/src/libiec61850.so
cmake --build /tmp/unitlab-iec61850-ied-build
ctest --test-dir /tmp/unitlab-iec61850-ied-build --output-on-failure
```

Do not ship a libIEC61850-linked binary as part of closed UnitLab runtime without a separate licensing decision.

## Dry Run

```bash
/tmp/unitlab-iec61850-ied-build/unitlab-iec61850-ied-sim \
  --fixture simulator/iec61850_ied/examples/single-report.fixture.json \
  --ied IED1 \
  --bind 127.0.0.1 \
  --port 1102 \
  --dry-run
```

The dry run only verifies that the fixture file is readable, contains the UnitLab fixture schema, and references the selected IED name.
It also reports the selected fixture shape:

```text
unitlab-iec61850-ied-sim: fixture accepted
schema=unitlab.iec61850.ied-simulator-fixture.v1
ied=IED1
accessPoint=AP1
devices=1
dataSets=1
reports=1
signals=2
firstDataSet=IED1/AP1/LD0/LLN0.dsEvents
firstSignal=LD0/XCBR1.Pos.stVal[ST]
firstReport=IED1/AP1/LD0/LLN0/brcbEvents/buffered
firstReportTriggerGI=true
firstReportOptDataRef=true
modelLogicalDevices=1
modelLogicalNodes=3
modelDataSets=1
modelReports=1
modelSignals=2
firstModelLogicalDevice=LD0
firstModelLogicalNode=LD0/LLN0
firstModelDataSet=LD0/LLN0.dsEvents
firstModelReport=IED1/AP1/LD0/LLN0/brcbEvents/buffered
firstModelReportRptID=IED1LD0/LLN0.BR.Events
firstModelReportBuffered=true
firstModelReportConfRevKnown=true
firstModelReportConfRev=7
firstModelReportBufTm=100
firstModelReportIntgPd=1000
firstModelReportTrgOpsMask=19
firstModelReportOptFldsMask=255
firstModelSignal=LD0/XCBR1.Pos.stVal[ST]
firstModelSignalKind=FCDA
firstModelSignalDO=Pos
firstModelSignalDA=stVal
firstModelSignalDataSetEntryVariable=LD0/XCBR1$ST$Pos$stVal
firstModelSignalDataSetEntryComponent=<none>
firstModelSignalValueKind=integer
firstModelSignalInitialValue=0
bind=127.0.0.1
port=1102
libiec61850=not-linked
```

## Fixture Contract

Schema: `unitlab.iec61850.ied-simulator-fixture.v1`

The fixture is produced by backend `build_ied_simulator_fixture_from_subscription_plan(...)`. It contains:

- IED name and access point name.
- DataSet references and ordered members.
- DataSet member initial values with preserved JSON value kinds.
- Optional DataSet member `component` metadata for array-member or component-specific fixtures.
- ReportControl metadata needed by the simulator: `RptID`, `ConfRev`, kind, indexed flag, `BufTm`, and `IntgPd`.
- Trigger options and optional fields.

It intentionally excludes:

- Signal List row IDs.
- FAT test-step state.
- Operator decisions.
- Runtime evidence.

Runtime evidence is captured separately by the C-owned runtime kernel through typed event records, pending-request correlation/timeout DTOs with per-request traces, semantic PDU structs, operation results, and a copy-safe runtime snapshot DTO for replay/debug capture. Report-control evidence is owned by `unitlab_iec61850_report_runtime.c`; session/request/transport evidence is owned by `unitlab_mms_core.c`. Report acceptance is modeled as `GI_PENDING -> REPORTING` before disable/release returns the control to `DISABLED`.

## Smoke Start

When linked with libIEC61850, `--smoke-start` builds the dynamic model, starts the MMS server, stops it immediately, and exits:

```bash
/tmp/unitlab-iec61850-ied-build/unitlab-iec61850-ied-sim \
  --fixture simulator/iec61850_ied/examples/single-report.fixture.json \
  --ied IED1 \
  --bind 127.0.0.1 \
  --port 1102 \
  --smoke-start
```

Expected result:

```text
unitlab-iec61850-ied-sim: server smoke-start accepted
ied=IED1
bind=127.0.0.1
port=1102
libiec61850=linked
```

When libIEC61850 is not linked, `--native-smoke-start` exercises the UnitLab-owned server runtime boundary directly, including the native lower-layer association ingress path:

```bash
/tmp/unitlab-iec61850-ied-build/unitlab-iec61850-ied-sim   --fixture simulator/iec61850_ied/examples/single-report.fixture.json   --ied IED1   --native-smoke-start
```

Expected result:

```text
unitlab-iec61850-ied-sim: native server smoke-start accepted
ied=IED1
bind=0.0.0.0
port=102
runtime=2
reportControl=4
```

## Non-Dry-Run Status

The non-dry-run path starts the linked MMS server and keeps the process alive until SIGTERM or SIGINT:

```bash
/tmp/unitlab-iec61850-ied-build/unitlab-iec61850-ied-sim \
  --fixture simulator/iec61850_ied/examples/single-report.fixture.json \
  --ied IED1 \
  --bind 127.0.0.1 \
  --port 1102
```

Expected current result without libIEC61850:

```text
LIBIEC61850_NOT_LINKED: libIEC61850 is not linked; build with UNITLAB_IEC61850_SIM_WITH_LIBIEC61850=ON before starting the MMS server.
libiec61850=not-linked
```

Expected current result with libIEC61850:

```text
<process keeps running until terminated>
```

The linked path creates the dynamic `IedModel`, logical devices, logical nodes, data objects, FCDA data attributes, DataSets, DataSet entries, ReportControls, and `IedServer`.

## Metadata Probe

When linked with libIEC61850, `--metadata-probe` connects to an already running simulator endpoint and verifies the server metadata against the fixture/model plan. It checks LD/DataSet/ReportControl directories, DataSet member counts and member order, core RCB attributes, `TrgOps`, `OptFlds`, and the initial disabled/unreserved RCB state:

```bash
/tmp/unitlab-iec61850-ied-build/unitlab-iec61850-ied-sim \
  --fixture simulator/iec61850_ied/examples/single-report.fixture.json \
  --ied IED1 \
  --bind 127.0.0.1 \
  --port 1102 \
  --metadata-probe
```

Expected result:

```text
unitlab-iec61850-ied-sim: metadata probe accepted
ied=IED1
endpoint=127.0.0.1:1102
dataSets=1
reports=1
libiec61850=linked
```

The backend uses this as an internal simulator readiness helper. It proves MMS metadata is readable from the external simulator, but it is not the production MMS adapter.

## GI Probe

When linked with libIEC61850, `--gi-probe` connects to an already running simulator endpoint and validates every ReportControl in the selected fixture device. For each ReportControl it enables reporting, requests GI, verifies the received report metadata, checks configured optional-field presence for `SeqNum`, `TimeOfEntry`, `DatSet`, `ReasonForInclusion`, `ConfRev`, and `DataRef`, checks the DataSet value count, checks every reported value against the fixture `initialValue` entries, disables the ReportControl, releases BRCB reservation, and re-reads the RCB cleanup state:

```bash
/tmp/unitlab-iec61850-ied-build/unitlab-iec61850-ied-sim \
  --fixture simulator/iec61850_ied/examples/single-report.fixture.json \
  --ied IED1 \
  --bind 127.0.0.1 \
  --port 1102 \
  --gi-probe
```

Expected result:

```text
unitlab-iec61850-ied-sim: GI probe accepted
ied=IED1
endpoint=127.0.0.1:1102
reports=1
libiec61850=linked
```

The probe is a simulator validation tool. It is not a substitute for the future backend MMS adapter and does not execute production subscriptions.

## Linked Client Smoke

When built with libIEC61850, CTest also runs `unitlab-iec61850-ied-linked-client-smoke`.

That test starts the simulator on `127.0.0.1`, connects with libIEC61850's `IedConnection` client API, and verifies:

- logical device discovery;
- DataSet directory discovery;
- DataSet member directory discovery;
- BRCB directory discovery;
- BRCB metadata reads for `RptID`, `DatSet`, `ConfRev`, `BufTm`, and `IntgPd`;
- BRCB and URCB GI request, report callback, configured optional-field presence, all fixture value validation, DataRef validation when enabled, and disabled/released cleanup validation.

This remains an internal simulator validation path. It does not make UnitLab production runtime depend on libIEC61850.

## Next Slice

The model plan now normalizes the fixture into the validated blueprint that the future libIEC61850 loader must consume:

- DataSet owner LD/LN/name parsed from the fixture reference.
- ReportControl owner LD/LN/name/kind/runtime attributes parsed from the fixture report metadata.
- DataSet member references parsed into LD/LN/member kind/component/object reference/data object/data attribute path/FC/libIEC61850 DataSetEntry variable/initial value kind/initial value.
- Fixture DataSet context and signal FC mismatches fail before server startup.
- Unsupported DataSet member kinds fail before server startup.
- Malformed or unknown initial value tokens fail before server startup.
- Unsupported report kinds and malformed `ConfRev` values fail before server startup.
- `TrgOps` and `OptFlds` are converted to the bit masks expected by libIEC61850 `ReportControlBlock_create`.
- DataSet entries are converted to the MMS variable-name form expected by libIEC61850 `DataSetEntry_create`.
- The linked loader now consumes the blueprint through libIEC61850 dynamic model APIs.

Continue the libIEC61850 server runtime from this model plan:

1. Wire the backend MMS adapter to the external simulator endpoint.
2. Add backend adapter coverage for read/reserve/enable/disable/GI once the MMS adapter contract is implemented.
3. Add typed FCD/CDC expansion from SCL `DataTypeTemplates`.
4. Keep all unsupported services fail-closed.
