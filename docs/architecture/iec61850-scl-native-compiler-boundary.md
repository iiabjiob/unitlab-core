# IEC 61850 SCL Native Compiler Boundary

Status: initial native compiler slice for the IEC 61850 v2 model path.

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
- `unitlab_scl_compile_model_plan(...)` for C-readable `UnitLabIedModelPlan` output
- C-readable diagnostics

Current implemented compiler coverage is intentionally narrow: selected IED, AccessPoint, Server, LDevice, LN0/LN, DataSet FCDA/FCD members, ReportControl, TrgOps, OptFields, and minimal DataTypeTemplates lookup for FCDA typed defaults. Invalid FCDA/FCD members are reported with `SCL_DATASET_MEMBER_INVALID` and are not emitted as runtime signals. ReportControl objects referencing missing datasets are reported with `SCL_REPORT_DATASET_MISSING` and are not emitted as runtime RCBs. Diagnostics carry C-readable context fields for IED, AccessPoint, logical device, logical node, DataSet, ReportControl, and member reference where available. The XML scan is still a temporary parser and must be replaced by a real C++ XML DOM layer before accepting broad SCD files.

## Next Compiler Slices

1. Replace the temporary XML scanner with a real parser in the C++ `scl-dom` layer.
2. Extend `DataTypeTemplates` resolution beyond direct `LNodeType -> DOType -> DA` lookup: DAType/BDA, SDO, EnumType, nested paths, and CDC-specific leaves.
3. Derive q/t metadata from resolved DO/DA definitions instead of fixture assumptions.
4. Add line/column or XPath location once the temporary XML scanner is replaced by a real parser.
5. Add an SLD compiler layer for Substation, VoltageLevel, Bay, ConductingEquipment, ConnectivityNode, Terminal, and graph topology.
6. Expose normalized JSON only as an adapter output; native runtime should consume typed compiled model structures.

## Non-goals

- No Python SCL parser as production path.
- No frontend SCD parser as source of truth.
- No C++ types crossing the public C runtime boundary.
