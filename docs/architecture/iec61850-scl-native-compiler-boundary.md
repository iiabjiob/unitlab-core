# IEC 61850 SCL Native Compiler Boundary

Status: initial native boundary for the IEC 61850 v2 model path.

## Decision

SCD/SCL and SLD normalization belong in the native IEC 61850 layer, not in the Python backend or frontend. The backend may own upload, persistence, project revision binding, and orchestration, but the source-of-truth compiler for IEC 61850 runtime models should be C++ with a stable C ABI.

## Boundary

- C++ owns SCL/SLD parsing and compilation internals.
- C owns the MMS server/runtime/report engine and talks to the compiler only through C-compatible handles and structs.
- Python backend should call the compiler through a shared library or CLI wrapper after persistence is added.
- Frontend consumes normalized/persisted model payloads and should not parse SCD as authoritative state.

Current native entrypoint:

- `iec61850_ied/src/scl_compiler/unitlab_scl_compiler.h`
- `unitlab_scl_compile_from_memory(...)`
- opaque `UnitLabSclCompileResult`
- C-readable diagnostics

## Next Compiler Slices

1. Replace the initial XML scanning scaffold with a real XML parser in the C++ `scl-dom` layer.
2. Add `DataTypeTemplates` resolution for LNodeType, DOType, DAType, EnumType.
3. Compile `IED/AccessPoint/Server/LDevice/LN0/LN`, datasets, FCDA/FCD, ReportControl, TrgOps, and OptFields into `UnitLabIedModelPlan`.
4. Add an SLD compiler layer for Substation, VoltageLevel, Bay, ConductingEquipment, ConnectivityNode, Terminal, and graph topology.
5. Expose normalized JSON only as an adapter output; native runtime should consume typed compiled model structures.

## Non-goals

- No Python SCL parser as production path.
- No frontend SCD parser as source of truth.
- No C++ types crossing the public C runtime boundary.
