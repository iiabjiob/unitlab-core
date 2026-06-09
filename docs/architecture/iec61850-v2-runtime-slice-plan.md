# IEC 61850 v2 Runtime Slice Plan

Status: planning document for v2 runtime work. This plan excludes topology/SLD rendering on purpose.

## Goal

v2 means a real SCD/SCL file can be compiled by the native IEC 61850 layer into a typed runtime model and used by the UnitLab-owned MMS server for discovery, read, report-control setup, GI, and data-change reports.

The source-of-truth compiler is C++ with a stable C ABI. Python backend may own upload, persistence, project/revision binding, and invocation, but it must not parse SCD/SCL as authoritative state. Frontend consumes normalized persisted output and does not parse SCD as source of truth.

## Explicit Non-goal

Topology/SLD is not part of this v2 runtime milestone.

Keep these artifacts separate:

- runtime model: IED, AccessPoint, logical devices, logical nodes, DataSets, FCDA/FCD members, ReportControl, TrgOps, OptFields, typed signals, q/t metadata;
- topology model: Substation, VoltageLevel, Bay, ConductingEquipment, Terminal, ConnectivityNode, SLD graph/layout.

The topology model should be a later compiler output from the same SCD input, not a field bolted onto the MMS runtime model.

## Current Baseline

Already implemented:

- native SCL compiler C API with opaque result and `UnitLabIedModelPlan` accessor;
- vendored `pugixml` DOM parser;
- selected IED / AccessPoint / Server / LDevice / LN0 / LN parsing;
- DataSet `FCDA` and `FCD` parsing;
- ReportControl parsing for `datSet`, `rptID`, `buffered`, `confRev`, `indexed`, `bufTime`, `intgPd`, `TrgOps`, and `OptFields`;
- basic diagnostics with C-readable context fields;
- typed defaults from direct `LNodeType -> DOType -> DA`, `DOType/SDO -> nested DOType -> DA`, nested `DAType/BDA` chains, derived `FCD` value/q/t leaves, and first `EnumVal` for enum defaults.

Known remaining gaps:

- C API diagnostics do not yet include line/column/XPath;
- native server smoke does not yet compile SCD directly into server runtime.

## Slice Plan

### Slice 1 - SCL DOM Layer Split

Status: implemented. `pugixml` traversal now lives in the internal `scl_dom` layer and the compiler consumes parsed DOM structs.

Move the current `pugixml` traversal out of `unitlab_scl_compiler.cpp` into a dedicated internal C++ SCL DOM layer.

Deliverables:

- internal `scl_dom` parser/types for IED/AP/server model and DataTypeTemplates;
- compiler file reduced to DOM-to-runtime-model compilation;
- existing C ABI unchanged;
- existing SCL compiler tests still pass.

Exit criteria:

- no behavior change in C API output;
- native compiler tests pass;
- DOM layer has focused tests or is covered through the compiler API test.

### Slice 2 - SDO Resolution

Status: implemented. Dataset FCDA members with dotted `doName` now resolve through `DOType/SDO -> nested DOType` before value-leaf typing. Unresolved SDO-backed members produce `SCL_DATASET_MEMBER_SDO_UNRESOLVED` diagnostics and are not emitted as runtime signals.

Deliverables:

- resolve dataset paths that cross SDO boundaries;
- preserve object reference formatting for nested DO paths;
- diagnostics for missing SDO type or unresolved SDO path.

Exit criteria:

- C API test covers at least one SDO-backed FCDA path;
- unresolved SDO does not silently fall back to integer without diagnostics.

### Slice 3 - Full Nested Attribute Expansion

Status: implemented. FCDA nested `daName` paths now traverse chained `DAType/BDA` templates beyond one struct level. Unresolved dotted data-attribute paths produce `SCL_DATASET_MEMBER_ATTRIBUTE_UNRESOLVED` diagnostics and are not emitted as runtime signals.

Deliverables:

- nested BDA chain traversal beyond one level;
- diagnostics for unresolved BDA path and missing DAType;
- typed defaults for resolved nested leaves.

Exit criteria:

- nested BDA test covers at least two struct levels;
- unresolved nested path produces structured diagnostic context.

### Slice 4 - q/t Metadata Derivation

Status: implemented for compiled `FCD` DataSet members. The compiler now expands `FCD` members through resolved `DOType/DA` definitions and emits ordered value/q/t runtime signals when the SCL template contains those leaves. This keeps q/t near the value leaf without changing the public C ABI.

Deliverables:

- identify value leaf plus sibling `q` and `t` attributes where present;
- model-plan representation for q/t metadata near the value leaf, or a clearly documented adapter strategy if model-plan shape must change later;
- report payload path continues to include q/t where expected.

Exit criteria:

- compiler test proves `stVal` can be associated with `q` and `t` from templates;
- native read/report tests still pass.

### Slice 5 - Template Diagnostics Hardening

Status: implemented. Missing `LNodeType`, `DOType`, `DAType`, `EnumType`, DO, DA, SDO, and BDA references now produce stable template diagnostics and do not emit runtime signals through silent default fallback.

Deliverables:

- diagnostics for missing `LNodeType`, `DOType`, `DAType`, `EnumType`, DO, DA, SDO, BDA;
- diagnostic context includes IED/AP/LD/LN/DataSet/member where available;
- no silent typed default when a template reference is expected but missing.

Exit criteria:

- negative C API tests cover each missing template kind;
- all diagnostics have stable codes.

### Slice 6 - C API Normalized Output

Status: implemented. The native compiler exposes a serialized normalized JSON adapter through C ABI size-query/write-buffer functions. JSON is generated with vendored `nlohmann/json`; C++ JSON types do not cross the public ABI.

Deliverables:

- `unitlab_scl_compile_normalized_json(...)` or equivalent size-query/write API;
- JSON schema string/version;
- diagnostics included in JSON projection;
- no C++ types cross ABI.

Exit criteria:

- C API test validates JSON shape and required fields;
- backend can call the compiler without knowing SCL internals.

### Slice 7 - Native SCD-to-Server Smoke

Status: implemented. A focused native smoke test now compiles an SCL fixture through the compiler C API, applies the resulting `UnitLabIedModelPlan` to `UnitLabMmsServerRuntime`, and validates discovery-style model lookup, signal/read lookup, and BRCB/DataSet references from the compiled model.

Deliverables:

- test fixture SCD with one BRCB and one DataSet;
- compile SCD -> model plan -> `unitlab_mms_server_runtime_apply_model_plan`;
- validate discovery/read/report-control lookup from compiled model.

Exit criteria:

- native C/C++ test proves SCD model drives server runtime without JSON fixture shortcuts;
- existing fixture-based tests still pass.

### Slice 8 - Report Payload From Compiled SCD Dataset

Status: implemented. The compiled-SCL runtime smoke now builds GI and data-change InformationReports from compiled DataSet membership. It verifies value/q/t DataRefs, changed-member inclusion bitstrings, and no fixture shortcuts.

Deliverables:

- GI report from compiled SCD dataset;
- data-change report from compiled SCD signal;
- ReasonForInclusion and inclusion bitstring align with dataset member positions;
- q/t metadata included when available.

Exit criteria:

- native report runtime test passes using SCD-compiled model;
- pcap/golden path is updated or planned for the compiled-SCD fixture.

### Slice 9 - Backend Compiler Invocation Boundary

Status: implemented. The backend has an IEC 61850 SCL import service and durable SQLAlchemy/Alembic repository that store source bytes, SHA-256 content hash, selected IED, normalized runtime payload, and compiler diagnostics. The upload endpoint calls the native compiler through the CLI adapter and does not parse SCL XML in Python.

Deliverables:

- backend stores SCD source and content hash through a durable repository;
- backend calls native compiler through CLI wrapper;
- backend persists normalized runtime payload and diagnostics;
- selected IED is explicit.

Exit criteria:

- backend test uploads/imports SCD and receives compiler diagnostics/model payload;
- backend does not parse XML itself.

### Slice 10 - Runtime Selection And Revision Binding

Status: implemented. Imported SCL models are not made active implicitly. A workspace-level runtime selection now points to a persisted SCL import, carries a monotonically increasing runtime revision when the selected import changes, and writes an append-only selection event with source hash, selected IED, schema, operator, and reason metadata.

Deliverables:

- workspace runtime revision association for imported SCD/SCL;
- active runtime model selection endpoint;
- audit trail for selected IED and source hash;
- future reports/test evidence can reference import id, SCD hash, selected IED, and runtime revision.

Exit criteria:

- backend service test proves model revision is explicit and auditable;
- no implicit replacement of active model.

### Slice 11 - Native Wire Golden Regression

Create golden regression coverage for compiled-SCD runtime behavior.

Deliverables:

- discovery pcap or decoded golden transcript;
- RptEna + GI pcap or decoded golden transcript;
- data-change pcap or decoded golden transcript;
- comparison script updated for compiled-SCD fixture.

Exit criteria:

- compiled-SCD path can be checked without manual packet inspection for every run;
- IEDScout/manual validation remains a secondary confirmation, not the only gate.

### Slice 12 - v2 Hardening Gate

Close reliability gaps before declaring v2 runtime usable.

Deliverables:

- malformed XML diagnostics;
- missing template diagnostics;
- unsupported SCL feature diagnostics;
- queue/backpressure policy still valid under compiled model;
- documented supported SCL profile.

Exit criteria:

- focused native and backend tests pass;
- unsupported features fail with stable diagnostics;
- docs name exactly what v2 runtime supports and does not support.

## Expected Slice Count

Runtime-only v2: about 12 focused slices from the current baseline.

If topology/SLD is added to the same milestone, add a separate topology compiler roadmap instead of extending this runtime plan.
