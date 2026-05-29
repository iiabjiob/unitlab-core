# UnitLab IEC 61850 IED Simulator

Status: internal test-tool scaffold. It does not run an MMS server yet.

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

## Fixture Contract

Schema: `unitlab.iec61850.ied-simulator-fixture.v1`

The fixture is produced by backend `build_ied_simulator_fixture_from_subscription_plan(...)`. It contains:

- IED name and access point name.
- DataSet references and ordered members.
- ReportControl metadata needed by the simulator.
- Trigger options and optional fields.

It intentionally excludes:

- Signal List row IDs.
- FAT test-step state.
- Operator decisions.
- Runtime evidence.

## Next Slice

Implement the libIEC61850 server model loader for one fixture device:

1. Load one `IedModel`.
2. Create one logical device and logical nodes.
3. Create one DataSet from fixture members.
4. Create one URCB/BRCB from fixture report metadata.
5. Support GI emission with fixture initial values.
6. Keep all unsupported services fail-closed.
