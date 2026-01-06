from datetime import datetime
from typing import Optional
from uuid import UUID

from pydantic import BaseModel, ConfigDict


class ProjectBase(BaseModel):
    name: str


class ProjectCreateSchema(ProjectBase):
    pass


class ProjectUpdateSchema(BaseModel):
    name: Optional[str] = None


class ProjectSchema(ProjectBase):
    id: int
    uuid: UUID
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True)
