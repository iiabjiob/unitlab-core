from __future__ import annotations

from pydantic import BaseModel, Field


class SignalImportMetaSchema(BaseModel):
    sheet_name: str | None = None
    source_sheet_name: str | None = None
    header_row_index: int | None = Field(default=None, ge=0)
    selected_columns: list[str] = Field(default_factory=list)
    terminal_column: str | None = None
    type_column: str | None = None
    type_mapping: dict[str, str] = Field(default_factory=dict)
    internal_type_column: str | None = None
