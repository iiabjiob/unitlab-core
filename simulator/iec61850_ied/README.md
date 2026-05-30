# UnitLab IEC 61850 IED Simulator

Status: internal test-tool scaffold. It validates the UnitLab fixture/model-plan/loader boundary, but it does not run an MMS server yet.

This directory is the boundary for the future libIEC61850-based IED simulator. It is intentionally separate from UnitLab backend/core runtime so GPL/native code cannot leak into production logic by accident.

## Purpose

- Consume the JSON fixture exported by backend IEC 61850 runtime.
- Later expose one simulated IED over MMS.
- Let UnitLab backend connect to the simulator through the same `Iec61850ClientAdapter` contract used for real IEDs.
- Keep Signal List matching, FAT evidence, diagnostics, and report planning owned by UnitLab.

## Build

Dry-run scaffold build without libIEC61850:

```bash
cmake -S simulator/iec61850_ied -B /tmp/unitlab-iec61850-ied-build
cmake --build /tmp/unitlab-iec61850-ied-build
```

Build with libIEC61850 availability check:

```bash
cmake -S simulator/iec61850_ied -B /tmp/unitlab-iec61850-ied-build \
  -DUNITLAB_IEC61850_SIM_WITH_LIBIEC61850=ON \
  -DLIBIEC61850_INCLUDE_DIR=/path/to/libiec61850/src/iec61850/inc \
  -DLIBIEC61850_LIBRARY=/path/to/libiec61850/build/libiec61850.a
cmake --build /tmp/unitlab-iec61850-ied-build
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
- ReportControl metadata needed by the simulator: `RptID`, `ConfRev`, kind, indexed flag, `BufTm`, and `IntgPd`.
- Trigger options and optional fields.

It intentionally excludes:

- Signal List row IDs.
- FAT test-step state.
- Operator decisions.
- Runtime evidence.

## Next Slice

The non-dry-run path validates inputs and fails closed until the loader is implemented:

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

## Next Slice

The model plan now normalizes the fixture into the validated blueprint that the future libIEC61850 loader must consume:

- DataSet owner LD/LN/name parsed from the fixture reference.
- ReportControl owner LD/LN/name/kind/runtime attributes parsed from the fixture report metadata.
- DataSet member references parsed into LD/LN/member kind/object reference/data object/data attribute path/FC/libIEC61850 DataSetEntry variable/initial value kind/initial value.
- Fixture DataSet context and signal FC mismatches fail before server startup.
- Unsupported DataSet member kinds fail before server startup.
- Malformed or unknown initial value tokens fail before server startup.
- Unsupported report kinds and malformed `ConfRev` values fail before server startup.
- `TrgOps` and `OptFlds` are converted to the bit masks expected by libIEC61850 `ReportControlBlock_create`.
- DataSet entries are converted to the MMS variable-name form expected by libIEC61850 `DataSetEntry_create`.

Implement the libIEC61850 server model loader from this model plan:

1. Load one `IedModel`.
2. Create one logical device and logical nodes.
3. Create one DataSet from fixture members.
4. Create one URCB/BRCB from fixture report metadata.
5. Support GI emission with fixture initial values.
6. Keep all unsupported services fail-closed.
