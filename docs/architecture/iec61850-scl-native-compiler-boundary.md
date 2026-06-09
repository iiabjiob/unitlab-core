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

Current implemented compiler coverage is intentionally narrow: selected IED, AccessPoint, Server, LDevice, LN0/LN, DataSet FCDA/FCD members, ReportControl, TrgOps, OptFields, and minimal DataTypeTemplates lookup for typed defaults, including derived `FCD` value/q/t leaves, `DOType/SDO` nested data-object paths, chained `DAType/BDA` nested attribute paths, and first-value `EnumType` defaults. Invalid FCDA/FCD members are reported with `SCL_DATASET_MEMBER_INVALID` and are not emitted as runtime signals. ReportControl objects referencing missing datasets are reported with `SCL_REPORT_DATASET_MISSING` and are not emitted as runtime RCBs. Diagnostics carry C-readable context fields for IED, AccessPoint, logical device, logical node, DataSet, ReportControl, and member reference where available. SCL XML is parsed through vendored `pugixml` in the internal `scl_dom` layer; malformed XML returns `SCL_XML_PARSE_FAILED` diagnostics instead of an empty silent model.

## Next Compiler Slices

1. Add source-location helpers to the internal `scl_dom` layer.
2. Extend `DataTypeTemplates` resolution beyond current `LNodeType -> DOType/SDO -> DA/DAType/BDA/EnumType` lookup: complete EnumVal metadata, template-specific diagnostics, and CDC-specific leaves.
3. Add line/column or XPath location on top of the `pugixml` DOM traversal.
4. Add an SLD compiler layer for Substation, VoltageLevel, Bay, ConductingEquipment, ConnectivityNode, Terminal, and graph topology.
5. Expose normalized JSON only as an adapter output; native runtime should consume typed compiled model structures.

## Non-goals

- No Python SCL parser as production path.
- No frontend SCD parser as source of truth.
- No C++ types crossing the public C runtime boundary.
