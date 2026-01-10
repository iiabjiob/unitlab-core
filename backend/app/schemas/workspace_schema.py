from datetime import datetime
from typing import Optional
from uuid import UUID

from pydantic import BaseModel, ConfigDict


class WorkspaceBase(BaseModel):
    name: str
    slug: str


class WorkspaceCreateSchema(WorkspaceBase):
    pass


class WorkspaceUpdateSchema(BaseModel):
    name: Optional[str] = None
    slug: Optional[str] = None


class WorkspaceSchema(WorkspaceBase):
    id: int
    uuid: UUID
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True)
