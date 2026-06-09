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
