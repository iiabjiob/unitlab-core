from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, Field


class VerificationTargetSchema(BaseModel):
    signal_id: int
    signal_reference: str
    signal_path: str
    endpoint_id: str | None = None
    expected_feedback_path: str | None = None
    timeout_ms: int = Field(default=5000, ge=1)
    window_ms: int = Field(default=1000, ge=0)
    protocol: str | None = None
    protocol_metadata: dict[str, Any] = Field(default_factory=dict)
    coverage_state: Literal["exact", "partial", "uncovered"] = "uncovered"
    coverage_reason: str | None = None
    allocation_id: int | None = None
    channel_id: int | None = None
    channel_label: str | None = None
    unit_id: str | None = None
    source_row_id: str | None = None


class VerificationSubscriptionPlanCoverageSchema(BaseModel):
    total_targets: int = 0
    covered_targets: int = 0
    partially_covered_targets: int = 0
    uncovered_targets: int = 0
    groups_count: int = 0
    endpoints_count: int = 0
    planning_quality: str = "partial"


class VerificationSubscriptionPlanSchema(BaseModel):
    selected_signal_ids: list[int] = Field(default_factory=list)
    targets: list[VerificationTargetSchema] = Field(default_factory=list)
    coverage: VerificationSubscriptionPlanCoverageSchema

