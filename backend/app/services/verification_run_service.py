from __future__ import annotations

from dataclasses import dataclass, replace
from contextlib import ExitStack
from pathlib import Path
import tempfile
from datetime import UTC, datetime
from typing import Callable
from uuid import uuid4

from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.signal_sheet.repository import SignalSheetRepository
from app.api.v1.signals.repository import SignalsRepository
from app.core.config import get_settings
from app.schemas.verification_schema import (
    VerificationAutoRunStartSchema,
    VerificationEvidenceDiagnosticSchema,
    VerificationExecutionContextSchema,
    VerificationRunDetailResponseSchema,
    VerificationRunSchema,
    VerificationSubscriptionPlanSchema,
    VerificationVerdictExplanationSchema,
)
from app.services.iec61850.mms_adapter import Iec61850MmsEndpointCatalog, build_mms_endpoint_catalog_from_scd_source
from app.services.iec61850.scl_import import Iec61850SqlAlchemySclImportRepository
from app.services.verification_endpoint_resolution import (
    build_verification_endpoint_resolution_diagnostic,
    resolve_verification_endpoint_resolution_policy,
)
from app.services.iec61850.report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportSubscriptionPlanDevice,
    build_simulator_endpoint_for_plan_device,
)
from app.services.iec61850.client_control import Iec61850ClientControlService
from app.services.verification_evidence import VerificationEvidenceRepository
from app.services.verification_execution import execute_verification_run
from app.services.verification_planner import (
    build_verification_subscription_plan,
    build_verification_target_sources,
)
from app.services.verification_signal_endpoint_catalog import build_verification_signal_endpoint_catalog
from app.services.verification_run_repository import VerificationRunRepository
from app.services.verification_runtime_selection import resolve_verification_runtime
from app.services.verification_runtime_selection import VerificationRuntimeSelection
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


@dataclass(frozen=True, slots=True)
class VerificationRuntimeStartContext:
    selected_signal_ids: list[int]
    subscription_plan: VerificationSubscriptionPlanSchema
    execution_context: VerificationExecutionContextSchema
    runtime_selection: VerificationRuntimeSelection
    diagnostics: tuple[VerificationEvidenceDiagnosticSchema, ...]


async def build_verification_runtime_start_context(
    *,
    workspace_id: int,
    payload: VerificationAutoRunStartSchema,
    db: AsyncSession,
    triggered_at: datetime | None = None,
) -> VerificationRuntimeStartContext:
    selected_signal_ids = [int(signal_id) for signal_id in payload.signal_ids if int(signal_id) > 0]
    if not selected_signal_ids:
        raise ValueError("Verification requires at least one selected signal.")

    start_at = triggered_at or payload.execution_context.triggered_at or datetime.now(UTC)
    execution_context = payload.execution_context.model_copy(
        update={
            "created_at": payload.execution_context.created_at or start_at,
            "triggered_at": payload.execution_context.triggered_at or start_at,
        }
    )

    signals_repo = SignalsRepository(db)
    sheet_repo = SignalSheetRepository(db)

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

    active_runtime_selection = None
    loaded_runtime_scd_endpoint_catalog = None
    loaded_runtime_scd_available = False
    runtime_mode = str(execution_context.runtime_version or "").strip().lower()
    if runtime_mode in {"mms", "live", "live-mms", "real-mms"} and hasattr(db, "execute"):
        scl_repository = Iec61850SqlAlchemySclImportRepository(db)
        active_runtime_selection = await scl_repository.get_active_runtime_selection(workspace_id=workspace_id)
        if active_runtime_selection is not None:
            loaded_runtime_scd_available = True
            source_bytes = await scl_repository.get_import_source(
                workspace_id=workspace_id,
                import_id=active_runtime_selection.import_id,
            )
            if source_bytes is not None:
                loaded_runtime_scd_endpoint_catalog = build_mms_endpoint_catalog_from_scd_source(
                    source_bytes,
                    selected_ied=active_runtime_selection.selected_ied,
                )

    endpoint_resolution_policy = resolve_verification_endpoint_resolution_policy(
        execution_context=execution_context,
        explicit_mms_endpoint_catalog=None,
        settings_mms_endpoint_catalog_json=getattr(get_settings(), "iec61850_mms_endpoint_catalog_json", None),
        loaded_runtime_scd_endpoint_catalog=loaded_runtime_scd_endpoint_catalog,
        loaded_runtime_scd_available=loaded_runtime_scd_available,
        active_runtime_selection_import_id=(
            active_runtime_selection.import_id if active_runtime_selection is not None else None
        ),
        active_runtime_selection_selected_ied=(
            active_runtime_selection.selected_ied if active_runtime_selection is not None else None
        ),
        active_runtime_selection_revision=(
            active_runtime_selection.runtime_revision if active_runtime_selection is not None else None
        ),
        transport_override_host=execution_context.transport_override_host,
        transport_override_port=execution_context.transport_override_port,
    )
    derived_signal_catalog = build_verification_signal_endpoint_catalog(
        sources=sources,
        subscription_plan=subscription_plan,
    )
    endpoint_resolution_diagnostics: list[VerificationEvidenceDiagnosticSchema] = []
    if endpoint_resolution_policy.endpoint_catalog is None and derived_signal_catalog is not None:
        endpoint_resolution_policy = replace(
            endpoint_resolution_policy,
            endpoint_catalog=derived_signal_catalog,
            transport_source="signal_list_fallback",
        )
        endpoint_resolution_diagnostics.append(
            VerificationEvidenceDiagnosticSchema(
                code="signal_list_endpoint_catalog_fallback",
                message="Signal list metadata supplied the MMS endpoint catalog fallback.",
                severity="info",
                details={
                    "group_count": len(subscription_plan.groups),
                },
            )
        )
    endpoint_resolution_diagnostic = build_verification_endpoint_resolution_diagnostic(endpoint_resolution_policy)

    runtime_selection = resolve_verification_runtime(
        execution_context=execution_context,
        now=lambda: start_at,
        endpoint_catalog=endpoint_resolution_policy.endpoint_catalog,
        transport_source=endpoint_resolution_policy.transport_source,
        model_source=endpoint_resolution_policy.model_source,
        transport_override_host=endpoint_resolution_policy.transport_override_host,
        transport_override_port=endpoint_resolution_policy.transport_override_port,
    )

    return VerificationRuntimeStartContext(
        selected_signal_ids=selected_signal_ids,
        subscription_plan=subscription_plan,
        execution_context=execution_context,
        runtime_selection=runtime_selection,
        diagnostics=(*endpoint_resolution_diagnostics, endpoint_resolution_diagnostic),
    )


async def execute_single_signal_verification_run(
    *,
    workspace_id: int,
    payload: VerificationAutoRunStartSchema,
    db: AsyncSession,
    triggered_at: datetime | None = None,
    client_id: str | None = None,
    endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device,
    mms_endpoint_catalog: Iec61850MmsEndpointCatalog | None = None,
    mms_control_service_factory: Callable[..., Iec61850ClientControlService] | None = None,
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
    runtime_mode = str(execution_context.runtime_version or "").strip().lower()
    active_runtime_selection = None
    loaded_runtime_scd_endpoint_catalog = None
    loaded_runtime_scd_available = False

    with ExitStack() as stack:
        loaded_runtime_scd_path: str | None = None
        if runtime_mode in {"mms", "live", "live-mms", "real-mms"} and hasattr(db, "execute"):
            scl_repository = Iec61850SqlAlchemySclImportRepository(db)
            active_runtime_selection = await scl_repository.get_active_runtime_selection(workspace_id=workspace_id)
            if active_runtime_selection is not None:
                loaded_runtime_scd_available = True
                source_bytes = await scl_repository.get_import_source(
                    workspace_id=workspace_id,
                    import_id=active_runtime_selection.import_id,
                )
                if source_bytes is not None:
                    loaded_runtime_scd_endpoint_catalog = build_mms_endpoint_catalog_from_scd_source(
                        source_bytes,
                        selected_ied=active_runtime_selection.selected_ied,
                    )
                    if loaded_runtime_scd_endpoint_catalog is not None:
                        temp_dir = stack.enter_context(tempfile.TemporaryDirectory(prefix="unitlab-mms-scd-"))
                        scl_path = Path(temp_dir) / f"{active_runtime_selection.selected_ied or 'runtime'}.scd"
                        scl_path.write_bytes(source_bytes)
                        loaded_runtime_scd_path = str(scl_path)

        endpoint_resolution_policy = resolve_verification_endpoint_resolution_policy(
            execution_context=execution_context,
            explicit_mms_endpoint_catalog=mms_endpoint_catalog,
            settings_mms_endpoint_catalog_json=getattr(get_settings(), "iec61850_mms_endpoint_catalog_json", None),
            loaded_runtime_scd_endpoint_catalog=loaded_runtime_scd_endpoint_catalog,
            loaded_runtime_scd_available=loaded_runtime_scd_available,
            active_runtime_selection_import_id=(
                active_runtime_selection.import_id if active_runtime_selection is not None else None
            ),
            active_runtime_selection_selected_ied=(
                active_runtime_selection.selected_ied if active_runtime_selection is not None else None
            ),
            active_runtime_selection_revision=(
                active_runtime_selection.runtime_revision if active_runtime_selection is not None else None
            ),
            transport_override_host=execution_context.transport_override_host,
            transport_override_port=execution_context.transport_override_port,
        )
        derived_signal_catalog = build_verification_signal_endpoint_catalog(
            sources=sources,
            subscription_plan=subscription_plan,
        )
        endpoint_resolution_diagnostics: list[VerificationEvidenceDiagnosticSchema] = []
        if endpoint_resolution_policy.endpoint_catalog is None and derived_signal_catalog is not None:
            endpoint_resolution_policy = replace(
                endpoint_resolution_policy,
                endpoint_catalog=derived_signal_catalog,
                transport_source="signal_list_fallback",
            )
            endpoint_resolution_diagnostics.append(
                VerificationEvidenceDiagnosticSchema(
                    code="signal_list_endpoint_catalog_fallback",
                    message="Signal list metadata supplied the MMS endpoint catalog fallback.",
                    severity="info",
                    details={
                        "group_count": len(subscription_plan.groups),
                    },
                )
            )
        endpoint_resolution_diagnostic = build_verification_endpoint_resolution_diagnostic(endpoint_resolution_policy)

        effective_mms_control_service_factory = mms_control_service_factory or Iec61850ClientControlService
        if loaded_runtime_scd_path is not None and effective_mms_control_service_factory is Iec61850ClientControlService:
            def _mms_control_service_factory_with_loaded_scd(**kwargs):
                kwargs.setdefault("target_scl_path", loaded_runtime_scd_path)
                return Iec61850ClientControlService(**kwargs)

            effective_mms_control_service_factory = _mms_control_service_factory_with_loaded_scd

        runtime_selection = resolve_verification_runtime(
            execution_context=execution_context,
            now=lambda: start_at,
            endpoint_catalog=endpoint_resolution_policy.endpoint_catalog,
            transport_source=endpoint_resolution_policy.transport_source,
            model_source=endpoint_resolution_policy.model_source,
            transport_override_host=endpoint_resolution_policy.transport_override_host,
            transport_override_port=endpoint_resolution_policy.transport_override_port,
            simulator_endpoint_for_device=endpoint_for_device,
            mms_control_service_factory=effective_mms_control_service_factory,
        )
        execution_result = await execute_verification_run(
            workspace_id=workspace_id,
            test_run_id=run_id,
            verification_targets=subscription_plan.targets,
            subscription_plan=subscription_plan,
            execution_context=execution_context,
            adapter=runtime_selection.adapter,
            endpoint_for_device=runtime_selection.endpoint_for_device,
            repository=evidence_repo,
            triggered_at=start_at,
            client_id=client_id or payload.client_id,
        )

        verification_run = execution_result.verification_run.model_copy(
            update={
                "diagnostics": [*execution_result.verification_run.diagnostics, *endpoint_resolution_diagnostics, endpoint_resolution_diagnostic],
            }
        )
        verdict_explanation = build_verification_verdict_explanation(
            verification_run=verification_run,
            verification_steps=verification_run.verification_steps,
            evidence_rows=execution_result.evidence_rows,
        )
        verification_run = verification_run.model_copy(
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
