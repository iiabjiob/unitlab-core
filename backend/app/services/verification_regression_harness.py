from __future__ import annotations

from dataclasses import asdict, dataclass
from datetime import UTC, datetime
from types import SimpleNamespace
from typing import Any, Callable, Literal, Sequence
from uuid import uuid4

from app.schemas.verification_schema import (
    SignalVerificationEvidenceSetSchema,
    VerificationExecutionContextSchema,
    VerificationConfidenceLevel,
    VerificationRunSchema,
    VerificationSessionSnapshotSchema,
    VerificationSubscriptionSnapshotSchema,
    VerificationSubscriptionPlanSchema,
    VerificationTargetSchema,
)
from app.services.iec61850.ied_simulator_fixture import (
    IED_SIMULATOR_FIXTURE_SCHEMA,
    build_ied_simulator_fixture_from_subscription_plan,
    ied_simulator_fixture_to_payload,
)
from app.services.verification_execution import build_runtime_subscription_plan, execute_simulated_verification_run
from app.services.verification_runtime_orchestrator import VerificationRuntimeOrchestrator


RegressionCaseMode = Literal["verification_run", "reconnect"]


@dataclass(frozen=True, slots=True)
class VerificationRegressionExpectation:
    verdict_state: str | None = None
    verification_confidence: VerificationConfidenceLevel | None = None
    confidence_reason: str | None = None
    runtime_state: str | None = None
    recovery_runtime_state: str | None = None
    recovery_reason: str | None = None
    evidence_count: int | None = None
    session_count: int | None = None
    subscription_count: int | None = None
    active_generation: int | None = None
    step_count: int | None = None


@dataclass(frozen=True, slots=True)
class VerificationRegressionCase:
    scenario_id: str
    mode: RegressionCaseMode
    description: str
    verification_targets: tuple[VerificationTargetSchema, ...]
    subscription_plan: VerificationSubscriptionPlanSchema
    execution_context: VerificationExecutionContextSchema
    expectation: VerificationRegressionExpectation
    client_id: str = "unitlab-regression"
    latency_ms: int = 250
    simulate_missing_signal_ids: tuple[int, ...] = ()
    simulate_stale_signal_ids: tuple[int, ...] = ()
    reconnect_session_index: int = 0


@dataclass(frozen=True, slots=True)
class VerificationRegressionResult:
    scenario_id: str
    mode: RegressionCaseMode
    passed: bool
    checks: tuple[str, ...]
    fixture_payload: dict[str, Any]
    report_payload: dict[str, Any]
    verification_run: VerificationRunSchema | None = None
    evidence_set: SignalVerificationEvidenceSetSchema | None = None
    session_snapshots: tuple[VerificationSessionSnapshotSchema, ...] = ()
    subscription_snapshots: tuple[VerificationSubscriptionSnapshotSchema, ...] = ()
    diagnostics: tuple[dict[str, Any], ...] = ()


@dataclass(frozen=True, slots=True)
class VerificationRegressionSuiteResult:
    suite_id: str
    passed: bool
    started_at: datetime
    finished_at: datetime
    results: tuple[VerificationRegressionResult, ...]
    summary: dict[str, Any]

    def to_report(self) -> dict[str, Any]:
        return {
            "suite_id": self.suite_id,
            "passed": self.passed,
            "started_at": self.started_at.isoformat().replace("+00:00", "Z"),
            "finished_at": self.finished_at.isoformat().replace("+00:00", "Z"),
            "summary": self.summary,
            "results": [result.report_payload for result in self.results],
        }


class _RegressionVerificationEvidenceRepository:
    def __init__(self) -> None:
        self.rows: list[dict[str, Any]] = []
        self.evidence_sets: list[dict[str, Any]] = []

    async def record_signal_verification_evidence(self, **kwargs: Any) -> Any:
        self.rows.append(dict(kwargs))
        return SimpleNamespace(**kwargs)

    async def upsert_signal_verification_evidence_set(self, **kwargs: Any) -> Any:
        self.evidence_sets.append(dict(kwargs))
        return SimpleNamespace(**kwargs)


async def run_verification_regression_case(
    case: VerificationRegressionCase,
    *,
    triggered_at: datetime | None = None,
    now: Callable[[], datetime] | None = None,
) -> VerificationRegressionResult:
    fixture_payload = _build_fixture_payload(case.subscription_plan)
    if case.mode == "verification_run":
        return await _run_verification_regression_case(
            case,
            fixture_payload=fixture_payload,
            triggered_at=triggered_at,
            now=now,
        )
    if case.mode == "reconnect":
        return _run_reconnect_regression_case(
            case,
            fixture_payload=fixture_payload,
            triggered_at=triggered_at,
            now=now,
        )
    raise ValueError(f'Unsupported regression case mode "{case.mode}".')


async def run_verification_regression_suite(
    cases: Sequence[VerificationRegressionCase],
    *,
    triggered_at: datetime | None = None,
    now: Callable[[], datetime] | None = None,
) -> VerificationRegressionSuiteResult:
    started_at = triggered_at or _now(now)
    results: list[VerificationRegressionResult] = []
    for case in cases:
        results.append(
            await run_verification_regression_case(
                case,
                triggered_at=started_at,
                now=now,
            )
        )
    finished_at = _now(now)
    passed = all(result.passed for result in results)
    summary = {
        "case_count": len(results),
        "passed_count": sum(1 for result in results if result.passed),
        "failed_count": sum(1 for result in results if not result.passed),
        "fixture_schema": IED_SIMULATOR_FIXTURE_SCHEMA,
    }
    return VerificationRegressionSuiteResult(
        suite_id=f"vsub-regression-{uuid4().hex[:8]}",
        passed=passed,
        started_at=started_at,
        finished_at=finished_at,
        results=tuple(results),
        summary=summary,
    )


async def _run_verification_regression_case(
    case: VerificationRegressionCase,
    *,
    fixture_payload: dict[str, Any],
    triggered_at: datetime | None,
    now: Callable[[], datetime] | None,
) -> VerificationRegressionResult:
    repo = _RegressionVerificationEvidenceRepository()
    started_at = triggered_at or _now(now)
    result = await execute_simulated_verification_run(
        workspace_id=1,
        test_run_id=case.scenario_id,
        verification_targets=case.verification_targets,
        subscription_plan=case.subscription_plan,
        execution_context=case.execution_context,
        repository=repo,  # type: ignore[arg-type]
        triggered_at=started_at,
        latency_ms=case.latency_ms,
        client_id=case.client_id,
        now=now,
        simulate_missing_signal_ids=case.simulate_missing_signal_ids,
        simulate_stale_signal_ids=case.simulate_stale_signal_ids,
    )
    checks = _compare_execution_expectation(
        case.expectation,
        result.verification_run,
        result.evidence_set,
    )
    report_payload = _build_run_case_report_payload(
        case=case,
        fixture_payload=fixture_payload,
        verification_run=result.verification_run,
        evidence_set=result.evidence_set,
        checks=checks,
        passed=not checks,
    )
    return VerificationRegressionResult(
        scenario_id=case.scenario_id,
        mode=case.mode,
        passed=not checks,
        checks=tuple(checks),
        fixture_payload=fixture_payload,
        report_payload=report_payload,
        verification_run=result.verification_run,
        evidence_set=result.evidence_set,
        session_snapshots=tuple(result.verification_run.session_snapshots),
        subscription_snapshots=tuple(result.verification_run.subscription_snapshots),
        diagnostics=tuple(_diagnostics_to_payload(result.verification_run.diagnostics)),
    )


def _run_reconnect_regression_case(
    case: VerificationRegressionCase,
    *,
    fixture_payload: dict[str, Any],
    triggered_at: datetime | None,
    now: Callable[[], datetime] | None,
) -> VerificationRegressionResult:
    orchestrator = VerificationRuntimeOrchestrator(now=now or (lambda: datetime.now(UTC)))
    start_at = triggered_at or _now(now)
    start_result = orchestrator.start(
        workspace_id=1,
        test_run_id=case.scenario_id,
        verification_targets=case.verification_targets,
        subscription_plan=case.subscription_plan,
        execution_context=case.execution_context,
        client_id=case.client_id,
    )
    session_id = start_result.session_snapshots[case.reconnect_session_index].session_id
    reconnect_result = orchestrator.reconnect(start_result.orchestration_id, session_id, workspace_id=1)
    verification_run = reconnect_result.verification_run
    checks = _compare_reconnect_expectation(case.expectation, verification_run, reconnect_result.session_snapshots, reconnect_result.subscription_snapshots)
    report_payload = _build_reconnect_case_report_payload(
        case=case,
        fixture_payload=fixture_payload,
        verification_run=verification_run,
        session_snapshots=reconnect_result.session_snapshots,
        subscription_snapshots=reconnect_result.subscription_snapshots,
        checks=checks,
        passed=not checks,
        started_at=start_at,
    )
    return VerificationRegressionResult(
        scenario_id=case.scenario_id,
        mode=case.mode,
        passed=not checks,
        checks=tuple(checks),
        fixture_payload=fixture_payload,
        report_payload=report_payload,
        verification_run=verification_run,
        session_snapshots=tuple(reconnect_result.session_snapshots),
        subscription_snapshots=tuple(reconnect_result.subscription_snapshots),
        diagnostics=tuple(_diagnostics_to_payload(verification_run.diagnostics)),
    )


def _compare_execution_expectation(
    expectation: VerificationRegressionExpectation,
    verification_run: VerificationRunSchema,
    evidence_set: SignalVerificationEvidenceSetSchema,
) -> list[str]:
    failures: list[str] = []
    if expectation.verdict_state is not None and verification_run.verdict_state != expectation.verdict_state:
        failures.append(f"verdict_state expected {expectation.verdict_state!r} got {verification_run.verdict_state!r}")
    if expectation.verification_confidence is not None and verification_run.verification_confidence != expectation.verification_confidence:
        failures.append(
            f"verification_confidence expected {expectation.verification_confidence!r} got {verification_run.verification_confidence!r}"
        )
    if expectation.confidence_reason is not None and verification_run.confidence_reason != expectation.confidence_reason:
        failures.append(f"confidence_reason expected {expectation.confidence_reason!r} got {verification_run.confidence_reason!r}")
    if expectation.runtime_state is not None and verification_run.runtime_state != expectation.runtime_state:
        failures.append(f"runtime_state expected {expectation.runtime_state!r} got {verification_run.runtime_state!r}")
    if expectation.recovery_reason is not None:
        recovery_reason = verification_run.recovery_state.recovery_reason if verification_run.recovery_state is not None else None
        if recovery_reason != expectation.recovery_reason:
            failures.append(f"recovery_reason expected {expectation.recovery_reason!r} got {recovery_reason!r}")
    if expectation.recovery_runtime_state is not None:
        recovery_runtime_state = verification_run.recovery_state.runtime_state if verification_run.recovery_state is not None else None
        if recovery_runtime_state != expectation.recovery_runtime_state:
            failures.append(
                f"recovery_runtime_state expected {expectation.recovery_runtime_state!r} got {recovery_runtime_state!r}"
            )
    if expectation.evidence_count is not None and evidence_set.summary.evidence_count != expectation.evidence_count:
        failures.append(f"evidence_count expected {expectation.evidence_count!r} got {evidence_set.summary.evidence_count!r}")
    if expectation.session_count is not None and len(verification_run.session_snapshots) != expectation.session_count:
        failures.append(f"session_count expected {expectation.session_count!r} got {len(verification_run.session_snapshots)!r}")
    if expectation.subscription_count is not None and len(verification_run.subscription_snapshots) != expectation.subscription_count:
        failures.append(
            f"subscription_count expected {expectation.subscription_count!r} got {len(verification_run.subscription_snapshots)!r}"
        )
    if expectation.step_count is not None and len(verification_run.verification_steps) != expectation.step_count:
        failures.append(f"step_count expected {expectation.step_count!r} got {len(verification_run.verification_steps)!r}")
    return failures


def _compare_reconnect_expectation(
    expectation: VerificationRegressionExpectation,
    verification_run: VerificationRunSchema,
    session_snapshots: Sequence[VerificationSessionSnapshotSchema],
    subscription_snapshots: Sequence[VerificationSubscriptionSnapshotSchema],
) -> list[str]:
    failures: list[str] = []
    if expectation.runtime_state is not None and verification_run.runtime_state != expectation.runtime_state:
        failures.append(f"runtime_state expected {expectation.runtime_state!r} got {verification_run.runtime_state!r}")
    if expectation.session_count is not None and len(session_snapshots) != expectation.session_count:
        failures.append(f"session_count expected {expectation.session_count!r} got {len(session_snapshots)!r}")
    if expectation.subscription_count is not None and len(subscription_snapshots) != expectation.subscription_count:
        failures.append(f"subscription_count expected {expectation.subscription_count!r} got {len(subscription_snapshots)!r}")
    if expectation.active_generation is not None:
        actual_generation = max((snapshot.connection_generation for snapshot in session_snapshots), default=1)
        if actual_generation != expectation.active_generation:
            failures.append(f"active_generation expected {expectation.active_generation!r} got {actual_generation!r}")
    if expectation.recovery_reason is not None:
        recovery_reason = verification_run.recovery_state.recovery_reason if verification_run.recovery_state is not None else None
        if recovery_reason != expectation.recovery_reason:
            failures.append(f"recovery_reason expected {expectation.recovery_reason!r} got {recovery_reason!r}")
    return failures


def _build_fixture_payload(subscription_plan: VerificationSubscriptionPlanSchema) -> dict[str, Any]:
    runtime_plan = build_runtime_subscription_plan(subscription_plan)
    fixture = build_ied_simulator_fixture_from_subscription_plan(runtime_plan)
    return ied_simulator_fixture_to_payload(fixture)


def _build_run_case_report_payload(
    *,
    case: VerificationRegressionCase,
    fixture_payload: dict[str, Any],
    verification_run: VerificationRunSchema,
    evidence_set: SignalVerificationEvidenceSetSchema,
    checks: Sequence[str],
    passed: bool,
) -> dict[str, Any]:
    return {
        "scenario_id": case.scenario_id,
        "mode": case.mode,
        "description": case.description,
        "passed": passed,
        "checks": list(checks),
        "fixture": fixture_payload,
        "verification_run": verification_run.model_dump(mode="json"),
        "evidence_set": evidence_set.model_dump(mode="json"),
        "expectation": asdict(case.expectation),
        "execution_context": case.execution_context.model_dump(mode="json"),
    }


def _build_reconnect_case_report_payload(
    *,
    case: VerificationRegressionCase,
    fixture_payload: dict[str, Any],
    verification_run: VerificationRunSchema,
    session_snapshots: Sequence[VerificationSessionSnapshotSchema],
    subscription_snapshots: Sequence[VerificationSubscriptionSnapshotSchema],
    checks: Sequence[str],
    passed: bool,
    started_at: datetime,
) -> dict[str, Any]:
    return {
        "scenario_id": case.scenario_id,
        "mode": case.mode,
        "description": case.description,
        "passed": passed,
        "checks": list(checks),
        "fixture": fixture_payload,
        "started_at": started_at.isoformat().replace("+00:00", "Z"),
        "verification_run": verification_run.model_dump(mode="json"),
        "session_snapshots": [snapshot.model_dump(mode="json") for snapshot in session_snapshots],
        "subscription_snapshots": [snapshot.model_dump(mode="json") for snapshot in subscription_snapshots],
        "expectation": asdict(case.expectation),
        "execution_context": case.execution_context.model_dump(mode="json"),
    }


def _diagnostics_to_payload(diagnostics: Sequence[Any]) -> list[dict[str, Any]]:
    payload: list[dict[str, Any]] = []
    for diagnostic in diagnostics:
        if hasattr(diagnostic, "model_dump"):
            payload.append(diagnostic.model_dump(mode="json"))
        elif isinstance(diagnostic, dict):
            payload.append(dict(diagnostic))
        else:
            payload.append({"value": str(diagnostic)})
    return payload


def _now(now: Callable[[], datetime] | None) -> datetime:
    return now() if now is not None else datetime.now(UTC)
