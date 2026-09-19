from typing import ClassVar

from datetime import datetime
from uuid import UUID

from pydantic import BaseModel, ConfigDict


class WorkspaceBase(BaseModel):
    name: str
    slug: str


class WorkspaceCreateSchema(WorkspaceBase):
    pass


class WorkspaceUpdateSchema(BaseModel):
    name: str | None = None
    slug: str | None = None


class WorkspaceSchema(WorkspaceBase):
    id: int
    uuid: UUID
    created_at: datetime
    updated_at: datetime

    model_config: ClassVar[ConfigDict] = ConfigDict(from_attributes=True)
