from __future__ import annotations

from pydantic import BaseModel, Field


class SignalImportVerificationColumnHintSchema(BaseModel):
    column: str | None = None
    confidence: str | None = None
    reason: str | None = None
    sample_values: list[str] = Field(default_factory=list)


class SignalImportVerificationSchema(BaseModel):
    enabled: bool = False
    transport_host_column: str | None = None
    iec61850_address_column: str | None = None
    transport_reference_column_hint: SignalImportVerificationColumnHintSchema = Field(
        default_factory=SignalImportVerificationColumnHintSchema,
    )
    transport_host_column_hint: SignalImportVerificationColumnHintSchema = Field(
        default_factory=SignalImportVerificationColumnHintSchema,
    )
    transport_port_column_hint: SignalImportVerificationColumnHintSchema = Field(
        default_factory=SignalImportVerificationColumnHintSchema,
    )
    ied_name_column_hint: SignalImportVerificationColumnHintSchema = Field(
        default_factory=SignalImportVerificationColumnHintSchema,
    )
    access_point_name_column_hint: SignalImportVerificationColumnHintSchema = Field(
        default_factory=SignalImportVerificationColumnHintSchema,
    )
    iec61850_address_column_hint: SignalImportVerificationColumnHintSchema = Field(
        default_factory=SignalImportVerificationColumnHintSchema,
    )
    logical_device_inst_column_hint: SignalImportVerificationColumnHintSchema = Field(
        default_factory=SignalImportVerificationColumnHintSchema,
    )
    logical_node_name_column_hint: SignalImportVerificationColumnHintSchema = Field(
        default_factory=SignalImportVerificationColumnHintSchema,
    )
    data_set_reference_column_hint: SignalImportVerificationColumnHintSchema = Field(
        default_factory=SignalImportVerificationColumnHintSchema,
    )
    report_control_reference_column_hint: SignalImportVerificationColumnHintSchema = Field(
        default_factory=SignalImportVerificationColumnHintSchema,
    )
    notes: list[str] = Field(default_factory=list)


class SignalImportMetaSchema(BaseModel):
    sheet_name: str | None = None
    source_sheet_name: str | None = None
    header_row_index: int | None = Field(default=None, ge=0)
    selected_columns: list[str] = Field(default_factory=list)
    terminal_column: str | None = None
    type_column: str | None = None
    type_mapping: dict[str, str] = Field(default_factory=dict)
    internal_type_column: str | None = None
    verification: SignalImportVerificationSchema | None = None
