from __future__ import annotations

from datetime import datetime
from typing import Any, ClassVar, Literal

from pydantic import BaseModel, ConfigDict, Field


SLD_DOCUMENT_SCHEMA = "unitlab.sld.v1"


class SldDocumentUpdateSchema(BaseModel):
    base_revision: int = Field(ge=0)
    document_schema: Literal["unitlab.sld.v1"] = SLD_DOCUMENT_SCHEMA
    document: dict[str, Any]
    change_kind: Literal["edit", "import", "migration"] = "edit"


class SldDocumentResponse(BaseModel):
    workspace_id: int
    revision: int
    document_schema: str
    document: dict[str, Any]
    updated_at: datetime | None

    model_config: ClassVar[ConfigDict] = ConfigDict(from_attributes=True)
