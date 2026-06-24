from __future__ import annotations

from dataclasses import dataclass
from datetime import UTC, datetime
from typing import Callable
from uuid import uuid4

from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.signal_sheet.repository import SignalSheetRepository
from app.api.v1.signals.repository import SignalsRepository
from app.schemas.verification_schema import (
    VerificationAutoRunStartSchema,
    VerificationRunDetailResponseSchema,
    VerificationRunSchema,
    VerificationVerdictExplanationSchema,
)
from app.services.iec61850.report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportSubscriptionPlanDevice,
    build_simulator_endpoint_for_plan_device,
)
from app.services.verification_evidence import VerificationEvidenceRepository
from app.services.verification_execution import execute_simulated_verification_run
from app.services.verification_planner import (
    build_verification_subscription_plan,
    build_verification_target_sources,
)
from app.services.verification_run_repository import VerificationRunRepository
from app.services.verification_verdict_explanation_service import build_verification_verdict_explanation


@dataclass(frozen=True, slots=True)
class VerificationAutoRunResult:
    test_run_id: str
    verification_run: VerificationRunSchema
    verdict_explanation: VerificationVerdictExplanationSchema

    def as_response(self) -> VerificationRunDetailResponseSchema:
        return VerificationRunDetailResponseSchema(
            test_run_id=self.test_run_id,
            verification_run=self.verification_run,
            verdict_explanation=self.verdict_explanation,
        )


async def execute_single_signal_verification_run(
    *,
    workspace_id: int,
    payload: VerificationAutoRunStartSchema,
    db: AsyncSession,
    triggered_at: datetime | None = None,
    client_id: str | None = None,
    endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device,
) -> VerificationAutoRunResult:
    selected_signal_ids = [int(signal_id) for signal_id in payload.signal_ids if int(signal_id) > 0]
    if not selected_signal_ids:
        raise ValueError("Verification requires at least one selected signal.")

    run_id = str(payload.test_run_id or f"vr-{uuid4().hex[:10]}")
    start_at = triggered_at or payload.execution_context.triggered_at or datetime.now(UTC)
    execution_context = payload.execution_context.model_copy(
        update={
            "created_at": payload.execution_context.created_at or start_at,
            "triggered_at": payload.execution_context.triggered_at or start_at,
        }
    )

    signals_repo = SignalsRepository(db)
    sheet_repo = SignalSheetRepository(db)
    evidence_repo = VerificationEvidenceRepository(db)
    run_repo = VerificationRunRepository(db)

    if not await signals_repo.ensure_workspace(workspace_id):
        raise ValueError("Workspace not found")

    signals = await signals_repo.list_by_ids(workspace_id, selected_signal_ids)
    if len(signals) != len(selected_signal_ids):
        found_ids = {signal.id for signal in signals}
        missing_ids = [signal_id for signal_id in selected_signal_ids if signal_id not in found_ids]
        raise ValueError(f"Unknown or inactive signal_id values: {missing_ids}")

    allocation_rows = await sheet_repo.list_allocation_rows_by_signal_ids(workspace_id, selected_signal_ids)
    signals_by_id = {signal.id: signal for signal in signals}
    allocation_rows_by_signal_id = {row.signal_id: row for row in allocation_rows}
    sources = build_verification_target_sources(
        requested_signal_ids=selected_signal_ids,
        signals_by_id=signals_by_id,
        allocation_rows_by_signal_id=allocation_rows_by_signal_id,
    )
    subscription_plan = build_verification_subscription_plan(sources)

    execution_result = await execute_simulated_verification_run(
        workspace_id=workspace_id,
        test_run_id=run_id,
        verification_targets=subscription_plan.targets,
        subscription_plan=subscription_plan,
        execution_context=execution_context,
        repository=evidence_repo,
        triggered_at=start_at,
        client_id=client_id or payload.client_id,
        endpoint_for_device=endpoint_for_device,
    )

    verdict_explanation = build_verification_verdict_explanation(
        verification_run=execution_result.verification_run,
        verification_steps=execution_result.verification_run.verification_steps,
        evidence_rows=execution_result.evidence_rows,
    )
    verification_run = execution_result.verification_run.model_copy(
        update={
            "reason": verdict_explanation.summary,
        }
    )
    response = VerificationRunDetailResponseSchema(
        test_run_id=run_id,
        verification_run=verification_run,
        verdict_explanation=verdict_explanation,
    )

    await run_repo.upsert_signal_verification_run(
        workspace_id=workspace_id,
        test_run_id=run_id,
        payload=response.model_dump(mode="json"),
    )
    await db.flush()
    return VerificationAutoRunResult(
        test_run_id=run_id,
        verification_run=verification_run,
        verdict_explanation=verdict_explanation,
    )


async def load_verification_run_detail(
    *,
    workspace_id: int,
    test_run_id: str,
    db: AsyncSession,
) -> VerificationRunDetailResponseSchema:
    run_repo = VerificationRunRepository(db)
    run = await run_repo.get_signal_verification_run(
        workspace_id=workspace_id,
        test_run_id=test_run_id,
    )
    if run is None:
        raise ValueError(f'Verification run "{test_run_id}" not found.')
    return VerificationRunDetailResponseSchema.model_validate(run.payload)
