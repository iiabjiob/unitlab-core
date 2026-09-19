from pydantic import BaseModel
from typing import Literal
from app.infrastructure.protocol.modes import Cmd
from app.infrastructure.protocol.modes import State
from enum import Enum

class WSAction(str, Enum):
    SET_DO_COMMAND  = "set_do_command"
    SET_AO_COMMAND  = "set_ao_command"
    GET_STATES      = "get_states"
    SCAN_DEVICES    = "scan_devices"
    ACK_CORE_DIAGNOSTICS = "ack_core_diagnostics"

# ---------------------------------------------------------------------
# Output control (DO / AO)
# ---------------------------------------------------------------------

class SetDoCommandMessage(BaseModel):
    """
    Manage digital outputs (DO).
    mode → matches protocol.modes.Cmd:
        - SET_SINGLE_BIT
        - SET_ALL_BIT
        - SET_PAIR_BIT
        - SET_PULSE_BIT
    """
    action: Literal[WSAction.SET_DO_COMMAND]
    workspace_id: int
    channel_id: int | None = None
    channel_ids: list[int] | None = None
    unit_id: str
    mode: Cmd
    ch: int | None = None
    value: int | None = None
    bitmask: int | None = None
    chA: int | None = None
    chB: int | None = None
    state2b: int | None = None
    pulse_ms: int = 0

class SetAoCommandMessage(BaseModel):
    """
    Manage analog outputs (AO).
    """
    action: Literal[WSAction.SET_AO_COMMAND]
    workspace_id: int
    channel_id: int
    unit_id: str
    ch: int
    value: float


# ---------------------------------------------------------------------
# State requests (DI / DO / AO)
# ---------------------------------------------------------------------

class RequestStateMessage(BaseModel):
    """
    Request the state of a device.
    mode → matches protocol.modes.State:
        - REQ_SINGLE_BIT / REQ_ALL_BIT
        - REQ_SINGLE_FLOAT / REQ_ALL_FLOAT
        - REQ_DIAG_DI_BIT / REQ_DIAG_ALL_BIT / REQ_DIAG_AO_FLOAT
    """
    action: Literal[WSAction.GET_STATES]
    unit_id: str
    mode: State
    ch: int | None = None


# ---------------------------------------------------------------------
# Device scan
# ---------------------------------------------------------------------

class ScanDevicesMessage(BaseModel):
    action: Literal[WSAction.SCAN_DEVICES]


class AcknowledgeCoreDiagnosticsMessage(BaseModel):
    action: Literal[WSAction.ACK_CORE_DIAGNOSTICS]
    hostname: str
    incident_id: str


# ---------------------------------------------------------------------
# Unified union
# ---------------------------------------------------------------------

WSMessage = (
    SetDoCommandMessage
    | SetAoCommandMessage
    | RequestStateMessage
    | ScanDevicesMessage
    | AcknowledgeCoreDiagnosticsMessage
)
