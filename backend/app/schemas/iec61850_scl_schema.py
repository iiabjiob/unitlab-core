from __future__ import annotations

from typing import Any

from pydantic import BaseModel, Field


class Iec61850SclDiagnosticSchema(BaseModel):
    severity: str
    code: str
    message: str
    ied_name: str = Field(default="", alias="iedName")
    access_point_name: str = Field(default="", alias="accessPointName")
    logical_device_inst: str = Field(default="", alias="logicalDeviceInst")
    logical_node_name: str = Field(default="", alias="logicalNodeName")
    data_set_name: str = Field(default="", alias="dataSetName")
    report_control_name: str = Field(default="", alias="reportControlName")
    member_reference: str = Field(default="", alias="memberReference")

    model_config = {"populate_by_name": True}


class Iec61850SclIedSummarySchema(BaseModel):
    name: str
    access_point_count: int = Field(alias="accessPointCount")

    model_config = {"populate_by_name": True}


class Iec61850SclIedDiscoveryResponseSchema(BaseModel):
    schema_: str = Field(alias="schema")
    source_size: int = Field(alias="sourceSize")
    ieds: list[Iec61850SclIedSummarySchema]
    diagnostics: list[Iec61850SclDiagnosticSchema]

    model_config = {"populate_by_name": True}


class Iec61850SclImportResponseSchema(BaseModel):
    import_id: str
    workspace_id: int
    source_filename: str | None
    source_hash: str
    source_size: int
    selected_ied: str
    normalized_schema: str
    normalized_model: dict[str, Any]
    diagnostics: list[Iec61850SclDiagnosticSchema]


class Iec61850SclImportFailureSchema(BaseModel):
    selected_ied: str
    code: str
    message: str


class Iec61850SclImportBatchResponseSchema(BaseModel):
    workspace_id: int
    source_filename: str | None
    source_size: int
    imports: list[Iec61850SclImportResponseSchema]
    failures: list[Iec61850SclImportFailureSchema]


class Iec61850SclImportListResponseSchema(BaseModel):
    workspace_id: int
    imports: list[Iec61850SclImportResponseSchema]


class Iec61850SclImportBatchJobStartResponseSchema(BaseModel):
    job_id: str
    total: int


class Iec61850SclImportBatchJobStatusSchema(BaseModel):
    job_id: str
    status: str
    workspace_id: int
    source_filename: str | None
    total: int
    current: int
    current_ied: str | None = None
    imports: list[Iec61850SclImportResponseSchema]
    failures: list[Iec61850SclImportFailureSchema]
    message: str | None = None


class Iec61850VirtualMmsServerStartRequestSchema(BaseModel):
    import_id: str
    host: str = "0.0.0.0"
    port: int = 12447


class Iec61850VirtualMmsServerStateSchema(BaseModel):
    running: bool
    import_id: str | None = None
    selected_ied: str | None = None
    source_hash: str | None = None
    host: str | None = None
    port: int | None = None
    pid: int | None = None
    fixture_path: str | None = None
    binary_path: str | None = None
    message: str | None = None



class Iec61850VirtualMmsServerLogsSchema(BaseModel):
    lines: list[str]


class Iec61850VirtualMmsRuntimeStatusSchema(BaseModel):
    running: bool
    data_client_connected: bool = False
    data_client: str = ""
    report_enabled: bool = False
    active_report: str = ""
    active_report_key: str = ""
    report_kind: str = ""
    report_id_reference: str = ""
    data_set_ref: str = ""
    data_set_reference: str = ""
    owner: str = ""
    pending_report_kind: str = "none"
    pending_report_queue_count: int = 0
    reports_sent: int = 0
    report_events_queued: int = 0


class Iec61850VirtualMmsSignalUpdateRequestSchema(BaseModel):
    object_reference: str
    value_kind: str
    value: Any


class Iec61850VirtualMmsSignalUpdateResponseSchema(BaseModel):
    ok: bool
    object_reference: str
    value_kind: str
    value: str
    report_queued: bool
    report_sent: bool
    pending_report_kind: str
    message: str


class Iec61850RuntimeSelectionRequestSchema(BaseModel):
    import_id: str
    selected_by: str | None = None
    reason: str | None = None


class Iec61850RuntimeSelectionResponseSchema(BaseModel):
    selection_id: str
    workspace_id: int
    import_id: str
    runtime_revision: int
    selected_ied: str
    source_hash: str
    normalized_schema: str
    selected_by: str | None = None
    selection_reason: str | None = None
