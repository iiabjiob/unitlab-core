# IEC 61850 SCL Backend Import

Status: first backend parser slice for the IEC 61850 v2 model path.

## Boundary

SCD/SCL XML is parsed on the backend. The frontend should upload the source file and consume the normalized backend payload instead of owning IEC 61850 model parsing rules.

Current endpoint:

- `POST /api/v1/iec61850/client/scl/import`
- multipart field `file`: UTF-8 SCD/SCL XML
- optional multipart field `selected_ied_name`: limits simulator fixture generation to one IED

The route currently lives under the existing IEC 61850 client router to avoid changing router registration during this slice. Before exposing this as a stable public API, move it to a neutral IEC 61850 model/import route such as `/api/v1/iec61850/scl/import`.

## Normalized Output

The import response returns:

- `model`: normalized SCL server model with IEDs, access points, communication addresses, logical devices, logical nodes, datasets, report controls, trigger options, optional fields, diagnostics, file name, and content hash.
- `simulatorFixture`: native IEC 61850 IED simulator fixture generated from the normalized model.
- `summary`: IED/device/error/warning counts for import UX and validation gates.

The normalized model schema is versioned as `unitlab.iec61850.scl.normalized.v1`.

## Current Coverage

Implemented parser coverage:

- `Communication/ConnectedAP/Address/P`
- `IED/AccessPoint/Server/LDevice`
- `LN0` and `LN`
- `DataSet` with `FCDA` and `FCD`
- `ReportControl`
- `TrgOps`
- `OptFields`

Malformed XML returns a normalized model with error diagnostics instead of throwing through the service layer.

## Next Steps

- Persist uploaded SCD source, content hash, selected IED, normalized model, and import diagnostics against a project/revision.
- Split the import endpoint out of the IEC 61850 client control router before treating it as public API.
- Extend parser coverage to DataTypeTemplates and map DO/DA definitions into stable signal metadata.
- Replace simulator placeholder initial values with typed defaults derived from the parsed data model.
