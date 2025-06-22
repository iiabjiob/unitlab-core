from pydantic import BaseModel, ConfigDict
from datetime import datetime
from typing import Optional

class SignalListEntryOut(BaseModel):
    id: int
    revision_id: int
    channel_index: int
    signal_type: Optional[str]
    terminal: Optional[str]
    bay_name: Optional[str]
    signal_name: Optional[str]
    hmi_presentation_text: Optional[str]
    group: Optional[str]
    reaction_matrix: Optional[str]
    external_address: Optional[int]

    model_config = ConfigDict(from_attributes=True)

class SignalListRevisionOut(BaseModel):
    id: int
    project_name: str
    version: str
    uploaded_at: datetime
    description: Optional[str]
    is_active: bool

    model_config = ConfigDict(from_attributes=True)
