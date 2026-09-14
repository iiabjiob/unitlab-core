from __future__ import annotations

import asyncio
import re
from datetime import datetime, timezone
from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest

from app.schemas.signal_sheet_schema import SignalAllocationRowSchema
from app.schemas.verification_schema import (
    SignalVerificationEvidenceSchema,
    VerificationEvidenceDiagnosticSchema,
    VerificationStepSchema,
)
from app.schemas.ws.events import SignalRowsPatchedEvent, SignalTestRuntimePatchEvent, WSChannel
from app.workers import signal_test_run_runner
from app.workers.signal_allocation_runner import _serialize_allocation_job_row_patches


def run_async(awaitable):
    return asyncio.run(awaitable)


class FakeNoRowsRepo:
    def __init__(self) -> None:
        self.db = FakeRepoDb()
        self.evidence: list[dict] = []

    async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids: list[int]):
        return []

    async def record_signal_test_run_step_evidence(self, **kwargs):
        self.evidence.append(dict(kwargs))


class FakeRepoDb:
    def add(self, item) -> None:
        del item

    async def flush(self) -> None:
        return None

    async def commit(self) -> None:
        return None

    async def rollback(self) -> None:
        return None

    async def execute(self, statement):
        del statement
        return SimpleNamespace(scalar=lambda: False)


@pytest.fixture(autouse=True)
def legacy_worker_contract_fixture(monkeypatch: pytest.MonkeyPatch):
    async def load_plan(repo, workspace_id: int, job_id: str):
        del job_id
        tracked_calls = getattr(repo, "calls", None)
        original_calls = list(tracked_calls) if tracked_calls is not None else None
        rows: list[SignalAllocationRowSchema] = []
        list_rows = getattr(repo, "list_allocation_rows_by_signal_ids", None)
        if list_rows is not None:
            for signal_id in (1, 2, 3, 4):
                rows.extend(await list_rows(workspace_id, [signal_id]))
        if tracked_calls is not None and original_calls is not None:
            tracked_calls[:] = original_calls
        if not rows:
            raise RuntimeError("Immutable test-run plan is missing")
        return rows

    async def acknowledge_commands(db, *, command_ids, timeout_ms):
        del db, timeout_ms
        return {str(command_id): "acknowledged" for command_id in command_ids}

    async def readback_ok(*args, **kwargs):
        del args, kwargs
        return True

    async def record_intent(*args, **kwargs):
        return SimpleNamespace(fencing_epoch=kwargs.get("fencing_epoch"))

    async def mark_queued(*args, **kwargs):
        return None

    async def deliver(db, *, command_id, action, command_sender):
        del db, action
        await command_sender(command_id)

    async def to_thread(func, /, *args, **kwargs):
        return func(*args, **kwargs)

    class Admission:
        def __init__(self, redis) -> None:
            self.redis = redis
            self.epoch = 0

        async def acquire(self, *, channel_id: int, owner_kind: str, owner_id: str):
            self.epoch += 1
            return SimpleNamespace(
                channel_id=channel_id,
                owner_kind=owner_kind,
                owner_id=owner_id,
                fencing_epoch=self.epoch,
            )

        async def release(self, lease):
            del lease
            return True

    async def schedule_discovery(**kwargs):
        del kwargs
        return None

    monkeypatch.setattr(signal_test_run_runner, "_load_immutable_plan_rows", load_plan)
    monkeypatch.setattr(signal_test_run_runner, "wait_for_hardware_command_acks", acknowledge_commands)
    monkeypatch.setattr(signal_test_run_runner, "_wait_for_bit_readback", readback_ok)
    monkeypatch.setattr(signal_test_run_runner, "_wait_for_float_readback", readback_ok)
    monkeypatch.setattr(signal_test_run_runner, "record_hardware_command_intent", record_intent)
    monkeypatch.setattr(signal_test_run_runner, "mark_hardware_command_intent_queued", mark_queued)
    monkeypatch.setattr(signal_test_run_runner, "_deliver_durable_command", deliver)
    monkeypatch.setattr(signal_test_run_runner, "HardwareCommandAdmission", Admission)
    monkeypatch.setattr(signal_test_run_runner, "_schedule_external_ied_discovery_for_verification_run", schedule_discovery)
    monkeypatch.setattr(signal_test_run_runner.asyncio, "to_thread", to_thread)


class FakeLiveRowsRepo:
    def __init__(self) -> None:
        self.db = FakeRepoDb()
        self.calls: list[list[int]] = []
        self.tested_at_by_signal: dict[int, str] = {}
        self.evidence: list[dict] = []

    async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids: list[int]):
        normalized_ids = [int(signal_id) for signal_id in signal_ids]
        self.calls.append(normalized_ids)
        if len(normalized_ids) != 1:
            raise AssertionError("test-run worker must resolve one current binding at execution time")
        signal_id = normalized_ids[0]
        return [build_allocation_row(signal_id)]

    async def mark_signals_tested_at(
        self,
        workspace_id: int,
        tested_at_by_signal: dict[int, str],
        *,
        commit: bool,
    ) -> list[int]:
        self.tested_at_by_signal.update(tested_at_by_signal)
        return sorted(tested_at_by_signal)

    async def record_signal_test_run_step_evidence(self, **kwargs):
        self.evidence.append(dict(kwargs))


class FakeRedis:
    def __init__(self) -> None:
        self._epochs: dict[str, int] = {}

    async def get(self, key: str):
        return "0"

    async def expire(self, key: str, ttl: int) -> None:
        return None

    async def set(self, key: str, value, **kwargs) -> bool:
        del key, value, kwargs
        return True

    async def incr(self, key: str) -> int:
        self._epochs[key] = self._epochs.get(key, 0) + 1
        return self._epochs[key]


def _valid_mms_plan_group() -> SimpleNamespace:
    return SimpleNamespace(
        group_id="group-1",
        source_classification="from discovery",
        report_control_name="brA",
        report_control_reference="IED-A/LLN0.brA",
        data_set_reference="IED-A/LLN0.dsA",
    )


def build_allocation_row(signal_id: int, **overrides) -> SignalAllocationRowSchema:
    data = {
        "row_id": f"signal-{signal_id}",
        "signal_id": signal_id,
        "signal_key": f"S{signal_id}",
        "signal_name": f"Signal {signal_id}",
        "signal_direction": "DI",
        "allocation_status": "assigned",
        "allocation_health": {"offline_device": False},
        "channel_id": 100 + signal_id,
        "device_id": 200 + signal_id,
        "channel_type": "do",
        "channel_index": signal_id,
        "channel_label": f"unit-{signal_id}/DO{signal_id}",
        "unit_id": f"unit-{signal_id}",
        "unit_online": True,
    }
    data.update(overrides)
    return SignalAllocationRowSchema(**data)


def test_serialize_allocation_job_row_patches_returns_json_safe_grid_patch_rows() -> None:
    rows = [
        SignalAllocationRowSchema(
            row_id="signal-1",
            signal_id=1,
            signal_key="S1",
            signal_name="Signal 1",
            signal_direction="DI",
            allocation_status="assigned",
            allocation_health={"offline_device": False},
            channel_id=10,
            tested_at=datetime(2026, 1, 1, 12, 30, tzinfo=timezone.utc),
        )
    ]

    payload = _serialize_allocation_job_row_patches(rows)

    assert payload[0]["row_id"] == "signal-1"
    assert payload[0]["signal_id"] == 1
    assert payload[0]["allocation_status"] == "assigned"
    assert payload[0]["allocation_health"] == {"offline_device": False}
    assert "signal_name" not in payload[0]
    assert isinstance(payload[0]["tested_at"], str)
    assert payload[0]["tested_at"].startswith("2026-01-01T12:30:00")


def test_signal_test_runtime_patch_event_serializes_tested_at_by_signal() -> None:
    event = SignalTestRuntimePatchEvent(
        job_id="job-1",
        workspace_id=7,
        tested_at_by_signal={1: "2026-01-01T12:30:00+00:00"},
        test_status_by_signal={1: "late"},
        emitted_at=datetime(2026, 1, 1, 12, 30, tzinfo=timezone.utc),
    )

    payload = event.model_dump(mode="json")

    assert payload["channel"] == WSChannel.SYSTEM_INFO.value
    assert payload["event"] == "signal_test_runtime_patch"
    assert payload["patch_type"] == "tested_at"
    assert payload["tested_at_by_signal"] == {"1": "2026-01-01T12:30:00+00:00"}
    assert payload["test_status_by_signal"] == {"1": "late"}
    assert payload["emitted_at"].startswith("2026-01-01T12:30:00")


def test_signal_test_status_derives_operator_verification_statuses() -> None:
    assert signal_test_run_runner._derive_test_status_from_verification(
        SimpleNamespace(evidence_status="observed", signal_value=True, diagnostics=[]),
        expected_value=True,
    ) == "verified"
    assert signal_test_run_runner._derive_test_status_from_verification(
        SimpleNamespace(evidence_status="late", signal_value=True, diagnostics=[]),
        expected_value=True,
    ) == "late"
    assert signal_test_run_runner._derive_test_status_from_verification(
        SimpleNamespace(evidence_status="timeout", signal_value=None, diagnostics=[]),
        expected_value=True,
    ) == "missing"
    assert signal_test_run_runner._derive_test_status_from_verification(
        SimpleNamespace(
            evidence_status="timeout",
            signal_value=None,
            diagnostics=[
                VerificationEvidenceDiagnosticSchema(
                    code="SIGNAL_NOT_INCLUDED_IN_REPORT_EVENT",
                    message="Different selected signal arrived.",
                )
            ],
        ),
        expected_value=True,
    ) == "unexpected"
    assert signal_test_run_runner._derive_test_status_from_verification(
        SimpleNamespace(evidence_status="observed", signal_value=False, diagnostics=[]),
        expected_value=True,
    ) == "inverted"


def test_step_evidence_status_does_not_hide_verdict_failure_after_ack() -> None:
    assert signal_test_run_runner._step_evidence_status("tested") == "succeeded"
    assert signal_test_run_runner._step_evidence_status("verified") == "succeeded"
    assert signal_test_run_runner._step_evidence_status("late") == "failed"
    assert signal_test_run_runner._step_evidence_status("not_validated") == "failed"
    assert signal_test_run_runner._step_evidence_status("value_mismatch") == "failed"


def test_terminal_job_status_does_not_promote_skips_or_verification_failures() -> None:
    base = {"processed": 2, "succeeded": 2, "skipped": 0, "verification_failed": 0}
    assert signal_test_run_runner._derive_terminal_job_status(base, progress_total=2) == "succeeded"
    assert signal_test_run_runner._derive_terminal_job_status(
        {**base, "skipped": 1}, progress_total=2
    ) == "failed"
    assert signal_test_run_runner._derive_terminal_job_status(
        {**base, "verification_failed": 1}, progress_total=2
    ) == "failed"
    assert signal_test_run_runner._derive_terminal_job_status(
        {**base, "succeeded": 1}, progress_total=2
    ) == "failed"


def test_sleep_before_restore_defers_task_cancellation(monkeypatch) -> None:
    async def cancelled_sleep(seconds: float) -> None:
        raise asyncio.CancelledError

    monkeypatch.setattr(signal_test_run_runner.asyncio, "sleep", cancelled_sleep)

    assert run_async(signal_test_run_runner._sleep_before_restore(1.0)) is True


def test_runner_exit_reconciles_unfinished_intents(monkeypatch) -> None:
    class FakeSession:
        async def __aenter__(self):
            return self

        async def __aexit__(self, exc_type, exc, traceback):
            return False

        async def commit(self):
            self.committed = True

    session = FakeSession()
    reconcile = AsyncMock(return_value=2)
    monkeypatch.setattr(signal_test_run_runner, "AsyncSessionLocal", lambda: session)
    monkeypatch.setattr(signal_test_run_runner, "reconcile_unfinished_hardware_command_intents", reconcile)

    assert run_async(
        signal_test_run_runner._reconcile_unfinished_intents_after_runner_exit(
            job_id="job-1",
            attempt_id="attempt-1",
        )
    ) == 2
    reconcile.assert_awaited_once_with(session, job_id="job-1", attempt_id="attempt-1")
    assert session.committed is True


def test_signal_rows_patched_event_serializes_grid_patch_contract() -> None:
    event = SignalRowsPatchedEvent(
        workspace_id=7,
        sequence=42,
        source="allocation",
        patches=[
            {
                "row_id": "signal-1",
                "signal_id": 1,
                "changes": {
                    "allocation_status": "assigned",
                    "channel_id": 10,
                },
                "columns": ["allocation_status", "channel_select"],
            }
        ],
        emitted_at=datetime(2026, 1, 1, 12, 30, tzinfo=timezone.utc),
    )

    payload = event.model_dump(mode="json")

    assert payload["channel"] == WSChannel.SYSTEM_INFO.value
    assert payload["event"] == "signal_rows_patched"
    assert payload["workspace_id"] == 7
    assert payload["sequence"] == 42
    assert payload["source"] == "allocation"
    assert payload["requires_full_reload"] is False
    assert payload["patches"] == [
        {
            "row_id": "signal-1",
            "signal_id": 1,
            "changes": {
                "allocation_status": "assigned",
                "channel_id": 10,
            },
            "columns": ["allocation_status", "channel_select"],
        }
    ]
    assert payload["emitted_at"].startswith("2026-01-01T12:30:00")


def test_signal_test_run_skips_missing_signal_without_sheet_revision_metadata(monkeypatch) -> None:
    async def publish_noop(event) -> None:
        return None

    payload = {
        "job_id": "job-1",
        "signal_ids": [1],
        "signal_interval_ms": 100,
        "toggle_mode": "single",
    }
    monkeypatch.setattr(signal_test_run_runner.RedisManager, "get_instance", lambda: FakeRedis())
    monkeypatch.setattr(signal_test_run_runner.WsEventPublisher, "publish", publish_noop)

    job_state = {
        "job_id": "job-1",
        "workspace_id": 7,
        "operation": "test_run",
        "status": "queued",
        "created_at": "2026-01-01T12:30:00+00:00",
    }

    repo = FakeNoRowsRepo()
    with pytest.raises(RuntimeError, match="Immutable test-run plan is missing"):
        run_async(signal_test_run_runner._handle_test_run(repo, 7, payload, job_state))  # type: ignore[arg-type]


def test_signal_test_run_resolves_current_binding_per_signal(monkeypatch) -> None:
    repo = FakeLiveRowsRepo()
    commands: list[tuple[str, dict]] = []

    async def publish_noop(event) -> None:
        return None

    async def sleep_noop(seconds: float) -> None:
        return None

    async def enqueue_do_noop(**kwargs) -> None:
        commands.append(("do", dict(kwargs)))

    async def enqueue_state_noop(**kwargs) -> None:
        commands.append(("state", dict(kwargs)))

    monkeypatch.setattr(signal_test_run_runner.RedisManager, "get_instance", lambda: FakeRedis())
    monkeypatch.setattr(signal_test_run_runner.WsEventPublisher, "publish", publish_noop)
    monkeypatch.setattr(signal_test_run_runner.asyncio, "sleep", sleep_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_do_command", enqueue_do_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_request_state", enqueue_state_noop)

    job_state = {
        "job_id": "job-1",
        "workspace_id": 7,
        "operation": "test_run",
        "status": "queued",
        "created_at": "2026-01-01T12:30:00+00:00",
    }
    payload = {
        "job_id": "job-1",
        "signal_ids": [1, 2],
        "signal_interval_ms": 100,
        "toggle_mode": "single",
    }

    result = run_async(
        signal_test_run_runner._handle_test_run(repo, 7, payload, job_state)  # type: ignore[arg-type]
    )

    assert repo.calls == [[1], [2]]
    assert [(kind, item["unit_id"], item["ch"]) for kind, item in commands if kind == "do"] == [
        ("do", "unit-1", 1),
        ("do", "unit-2", 2),
    ]
    assert result["succeeded"] == 2
    assert result["skipped"] == 0
    assert result["evidence_count"] == 2
    assert sorted(repo.tested_at_by_signal) == [1, 2]
    assert [item["status"] for item in repo.evidence] == ["succeeded", "succeeded"]
    assert repo.evidence[0]["signal_id"] == 1
    assert repo.evidence[0]["channel_id"] == 101
    assert repo.evidence[0]["unit_id"] == "unit-1"
    assert repo.evidence[0]["result_state"] == "commands_enqueued"
    assert repo.evidence[0]["command_payload"]["commands"][0]["kind"] == "do_set"


def test_signal_test_run_requires_iec61850_report_when_verification_enabled(monkeypatch) -> None:
    commands: list[tuple[str, dict]] = []
    persisted_verification: list[dict] = []

    class MappedRowsRepo(FakeLiveRowsRepo):
        async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids: list[int]):
            self.calls.append([int(signal_id) for signal_id in signal_ids])
            return [
                build_allocation_row(
                    int(signal_id),
                    signal_metadata={
                        "verification": {
                            "enabled": True,
                            "transport_host": "172.16.40.128:12447",
                            "iec61850_address": "LD0/XCBR1.Pos.stVal[ST]",
                        }
                    },
                )
                for signal_id in signal_ids
            ]

    async def publish_noop(event) -> None:
        return None

    async def enqueue_do_noop(**kwargs) -> None:
        commands.append(("do", dict(kwargs)))

    async def enqueue_state_noop(**kwargs) -> None:
        commands.append(("state", dict(kwargs)))

    async def build_context_noop(**kwargs):
        return SimpleNamespace(
            subscription_plan=SimpleNamespace(targets=[], groups=[_valid_mms_plan_group()]),
            execution_context=kwargs["payload"].execution_context,
            runtime_selection=SimpleNamespace(adapter=object(), endpoint_for_device=lambda device: device),
            diagnostics=(),
        )

    class FakeVerificationEvidenceRepository:
        def __init__(self, db) -> None:
            self.db = db

        async def record_signal_verification_evidence(self, **kwargs):
            persisted_verification.append({"kind": "row", **kwargs})

        async def upsert_signal_verification_evidence_set(self, **kwargs):
            persisted_verification.append({"kind": "set", **kwargs})

    class FakeVerificationRuntimeOrchestrator:
        def start(self, **kwargs):
            return SimpleNamespace(orchestration_id="local-orch-1")

        def capture_triggered_signal(self, orchestration_id: str, **kwargs):
            evidence = SignalVerificationEvidenceSchema(
                evidence_id="ev-1",
                signal_id=int(kwargs["signal_id"]),
                signal_path="breaker_close",
                expected_path="LD0/XCBR1.Pos.stVal[ST]",
                actual_report_path="LD0/XCBR1.Pos.stVal[ST]",
                source_ied="IED-A",
                endpoint_id="sim:IED-A/P1",
                rpt_id="IED-A/LLN0.brA",
                dataset="IED-A/LLN0.dsA",
                observed_at=datetime(2026, 1, 1, 12, 30, tzinfo=timezone.utc),
                latency_ms=1,
                quality="good",
                freshness="live",
                evidence_status="observed",
                reason_code="report_received",
                source_generation=1,
                source_report_sequence_generation=2,
                source_report_sequence_number=2,
                report_reason="data-change",
                signal_value=1,
                timestamp_summary={"observed_at": "2026-01-01T12:30:00+00:00"},
                evidence_kind="report_observation",
            )
            step = VerificationStepSchema(
                step_id="step-1",
                signal_id=int(kwargs["signal_id"]),
                target_index=0,
                session_id="job-1:sim:IED-A/P1",
                subscription_id="job-1:group-1",
                group_id="group-1",
                step_state="completed",
                expected_path=evidence.expected_path,
                expected_window_ms=1000,
                freshness="live",
                evidence_status="observed",
                verdict_state="pass",
                evidence_ids=[evidence.evidence_id],
                actual_report_path=evidence.actual_report_path,
                source_session_id="job-1:sim:IED-A/P1",
                source_subscription_id="job-1:group-1",
                source_generation=1,
                source_report_rpt_id=evidence.rpt_id,
                source_report_dat_set=evidence.dataset,
                triggered_at=kwargs["triggered_at"],
                observed_at=evidence.observed_at,
                latency_ms=1,
                reason="report_received",
            )
            return SimpleNamespace(evidence=evidence, step=step, diagnostics=())

        def stop(self, orchestration_id: str):
            return SimpleNamespace()

    monkeypatch.setattr(signal_test_run_runner.RedisManager, "get_instance", lambda: FakeRedis())
    monkeypatch.setattr(signal_test_run_runner.WsEventPublisher, "publish", publish_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_do_command", enqueue_do_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_request_state", enqueue_state_noop)
    monkeypatch.setattr(signal_test_run_runner, "build_verification_runtime_start_context", build_context_noop)
    monkeypatch.setattr(signal_test_run_runner, "VerificationEvidenceRepository", FakeVerificationEvidenceRepository)
    monkeypatch.setattr(signal_test_run_runner, "VerificationRuntimeOrchestrator", FakeVerificationRuntimeOrchestrator)

    job_state = {
        "job_id": "job-1",
        "workspace_id": 7,
        "operation": "test_run",
        "status": "queued",
        "created_at": "2026-01-01T12:30:00+00:00",
    }
    payload = {
        "job_id": "job-1",
        "signal_ids": [1],
        "signal_interval_ms": 100,
        "toggle_mode": "single",
        "verification_enabled": True,
        "verification_runtime_version": "mms",
        "verification_orchestration_id": "api-orch-1",
        "verification_signal_list_revision_id": 2,
    }

    repo = MappedRowsRepo()
    result = run_async(signal_test_run_runner._handle_test_run(repo, 7, payload, job_state))  # type: ignore[arg-type]

    assert result["succeeded"] == 1
    assert result["verification_observed"] == 1
    assert result["verification_failed"] == 0
    assert result["test_status_by_signal"] == {1: "verified"}
    assert result["test_report_by_signal"][1]["test_status"] == "verified"
    assert result["test_report_by_signal"][1]["iec61850_address"] == "LD0/XCBR1.Pos.stVal[ST]"
    assert result["test_report_by_signal"][1]["iec61850_verification"]["timeout_ms"] == 5000
    assert result["test_report_by_signal"][1]["iec61850_verification"]["triggered_at"]
    assert result["test_report_by_signal"][1]["iec61850_verification"]["evidence"]["actual_report_path"] == "LD0/XCBR1.Pos.stVal[ST]"
    assert result["test_report_by_signal"][1]["command"]["expected_feedback_value"] == 1
    assert repo.evidence[0]["status"] == "succeeded"
    assert repo.evidence[0]["result_state"] == "commands_enqueued_report_verified"
    assert repo.evidence[0]["command_payload"]["iec61850_verification"]["requested_online_orchestration_id"] == "api-orch-1"
    assert persisted_verification[0]["kind"] == "row"
    assert persisted_verification[-1]["kind"] == "set"


def test_signal_test_run_publishes_iec61850_preparation_steps_before_commands(monkeypatch) -> None:
    commands: list[tuple[str, dict]] = []
    published_events: list[dict] = []

    class MappedRowsRepo(FakeLiveRowsRepo):
        async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids: list[int]):
            self.calls.append([int(signal_id) for signal_id in signal_ids])
            return [
                build_allocation_row(
                    int(signal_id),
                    signal_metadata={
                        "verification": {
                            "enabled": True,
                            "transport_host": "172.16.40.128:12447",
                            "iec61850_address": "LD0/XCBR1.Pos.stVal[ST]",
                        }
                    },
                )
                for signal_id in signal_ids
            ]

    async def publish_capture(event) -> None:
        published_events.append(event.model_dump(mode="json") if hasattr(event, "model_dump") else dict(event))

    async def sleep_noop(seconds: float) -> None:
        return None

    async def enqueue_do_noop(**kwargs) -> None:
        commands.append(("do", dict(kwargs)))

    async def enqueue_state_noop(**kwargs) -> None:
        commands.append(("state", dict(kwargs)))

    async def build_context_noop(**kwargs):
        return SimpleNamespace(
            subscription_plan=SimpleNamespace(targets=[object()], groups=[_valid_mms_plan_group()]),
            execution_context=kwargs["payload"].execution_context,
            runtime_selection=SimpleNamespace(adapter=object(), endpoint_for_device=lambda device: device),
            diagnostics=(),
        )

    class FakeVerificationEvidenceRepository:
        def __init__(self, db) -> None:
            self.db = db

        async def record_signal_verification_evidence(self, **kwargs):
            return None

        async def upsert_signal_verification_evidence_set(self, **kwargs):
            return None

    class FakeVerificationRuntimeOrchestrator:
        def __init__(self) -> None:
            self.snapshot_calls = 0

        def start_deferred(self, **kwargs):
            return SimpleNamespace(orchestration_id="local-orch-1")

        def snapshot(self, orchestration_id: str):
            self.snapshot_calls += 1
            reporting = self.snapshot_calls >= 2
            return SimpleNamespace(
                verification_run=SimpleNamespace(runtime_state="reporting" if reporting else "connecting"),
                session_snapshots=[
                    SimpleNamespace(runtime_state="reporting" if reporting else "connecting"),
                ],
                subscription_snapshots=[
                    SimpleNamespace(
                        subscription_id="sub-1",
                        endpoint_id="172.16.40.128:12447",
                        group_id="group-1",
                        report_control_name="brcbA",
                        report_control_reference="LD0/LLN0.BR.brcbA",
                        data_set_reference="LD0/LLN0.dsA",
                        subscription_state="reporting" if reporting else "pending",
                        report_health="healthy" if reporting else "pending",
                        gi_requested=reporting,
                        last_report_value_count=3 if reporting else 0,
                        last_error=None,
                        diagnostic_code=None,
                    )
                ],
            )

        def capture_triggered_signal(self, orchestration_id: str, **kwargs):
            evidence = SignalVerificationEvidenceSchema(
                evidence_id="ev-1",
                signal_id=int(kwargs["signal_id"]),
                signal_path="breaker_close",
                expected_path="LD0/XCBR1.Pos.stVal[ST]",
                actual_report_path="LD0/XCBR1.Pos.stVal[ST]",
                observed_at=datetime(2026, 1, 1, 12, 30, tzinfo=timezone.utc),
                latency_ms=1,
                freshness="live",
                evidence_status="observed",
                reason_code="report_received",
            )
            step = VerificationStepSchema(
                step_id="step-1",
                signal_id=int(kwargs["signal_id"]),
                target_index=0,
                session_id="session-1",
                subscription_id="sub-1",
                group_id="group-1",
                step_state="completed",
                expected_path=evidence.expected_path,
                expected_window_ms=1000,
                freshness="live",
                evidence_status="observed",
                verdict_state="pass",
                evidence_ids=[evidence.evidence_id],
                triggered_at=kwargs["triggered_at"],
                observed_at=evidence.observed_at,
                latency_ms=1,
                reason="report_received",
            )
            return SimpleNamespace(evidence=evidence, step=step, diagnostics=())

        def stop(self, orchestration_id: str):
            return SimpleNamespace()

    monkeypatch.setattr(signal_test_run_runner.RedisManager, "get_instance", lambda: FakeRedis())
    monkeypatch.setattr(signal_test_run_runner.WsEventPublisher, "publish", publish_capture)
    monkeypatch.setattr(signal_test_run_runner.asyncio, "sleep", sleep_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_do_command", enqueue_do_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_request_state", enqueue_state_noop)
    monkeypatch.setattr(signal_test_run_runner, "build_verification_runtime_start_context", build_context_noop)
    monkeypatch.setattr(signal_test_run_runner, "VerificationEvidenceRepository", FakeVerificationEvidenceRepository)
    monkeypatch.setattr(signal_test_run_runner, "VerificationRuntimeOrchestrator", FakeVerificationRuntimeOrchestrator)

    job_state = {
        "job_id": "job-1",
        "workspace_id": 7,
        "operation": "test_run",
        "status": "queued",
        "created_at": "2026-01-01T12:30:00+00:00",
    }
    payload = {
        "job_id": "job-1",
        "signal_ids": [1],
        "signal_interval_ms": 100,
        "toggle_mode": "single",
        "verification_enabled": True,
        "verification_runtime_version": "mms",
        "verification_signal_list_revision_id": 2,
    }

    result = run_async(signal_test_run_runner._handle_test_run(MappedRowsRepo(), 7, payload, job_state))  # type: ignore[arg-type]

    prepare_results = [
        event.get("result") for event in published_events
        if isinstance(event.get("result"), dict) and event["result"].get("phase") == "preparing_iec61850"
    ]
    assert result["verification_observed"] == 1
    assert [item[0] for item in commands[:2]] == ["state", "do"]
    assert commands[0][1]["mode"].name == "REQ_ALL_BIT"
    assert prepare_results
    assert prepare_results[-1]["verification_prepare_steps"][-1]["id"] == "start_test"
    assert prepare_results[-1]["verification_prepare_steps"][-1]["status"] == "done"
    assert "verification_prepare_subscriptions" not in prepare_results[-1]
    prepare_messages = [
        str(event.get("message") or "")
        for event in published_events
        if isinstance(event.get("result"), dict) and event["result"].get("phase") == "preparing_iec61850"
    ]
    assert prepare_messages
    assert not any(re.search(r"\d+\s*/\s*\d+", message) for message in prepare_messages)


def test_signal_test_run_verifies_only_mapped_iec61850_rows(monkeypatch) -> None:
    rows_by_signal_id = {
        1: build_allocation_row(
            1,
            signal_metadata={
                "verification": {
                    "enabled": True,
                    "transport_host": "172.16.40.128:12447",
                    "iec61850_address": "IEDLD0/GGIO1.stVal[ST]",
                }
            },
        ),
        2: build_allocation_row(2, signal_metadata={}),
    }
    commands: list[tuple[str, dict]] = []
    context_signal_ids: list[list[int]] = []
    captured_signal_ids: list[int] = []

    class MixedRowsRepo(FakeLiveRowsRepo):
        async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids: list[int]):
            self.calls.append([int(signal_id) for signal_id in signal_ids])
            return [rows_by_signal_id[int(signal_id)] for signal_id in signal_ids if int(signal_id) in rows_by_signal_id]

    async def publish_noop(event) -> None:
        return None

    async def enqueue_do_noop(**kwargs) -> None:
        commands.append(("do", dict(kwargs)))

    async def enqueue_state_noop(**kwargs) -> None:
        commands.append(("state", dict(kwargs)))

    async def build_context_noop(**kwargs):
        context_signal_ids.append(list(kwargs["payload"].signal_ids))
        return SimpleNamespace(
            subscription_plan=SimpleNamespace(targets=[], groups=[_valid_mms_plan_group()]),
            execution_context=kwargs["payload"].execution_context,
            runtime_selection=SimpleNamespace(adapter=object(), endpoint_for_device=lambda device: device),
            diagnostics=(),
        )

    class FakeVerificationRuntimeOrchestrator:
        def start(self, **kwargs):
            return SimpleNamespace(orchestration_id="local-orch-1")

        def capture_triggered_signal(self, orchestration_id: str, **kwargs):
            captured_signal_ids.append(int(kwargs["signal_id"]))
            evidence = SignalVerificationEvidenceSchema(
                evidence_id=f"ev-{kwargs['signal_id']}",
                signal_id=int(kwargs["signal_id"]),
                signal_path="sig",
                expected_path="IEDLD0/GGIO1.stVal[ST]",
                actual_report_path="IEDLD0/GGIO1.stVal[ST]",
                observed_at=datetime(2026, 1, 1, 12, 30, tzinfo=timezone.utc),
                latency_ms=1,
                freshness="live",
                evidence_status="observed",
                reason_code="report_received",
            )
            step = VerificationStepSchema(
                step_id=f"step-{kwargs['signal_id']}",
                signal_id=int(kwargs["signal_id"]),
                target_index=0,
                session_id="session-1",
                subscription_id="sub-1",
                group_id="group-1",
                step_state="completed",
                expected_path=evidence.expected_path,
                expected_window_ms=1000,
                freshness="live",
                evidence_status="observed",
                verdict_state="pass",
                evidence_ids=[evidence.evidence_id],
                triggered_at=kwargs["triggered_at"],
                observed_at=evidence.observed_at,
                latency_ms=1,
                reason="report_received",
            )
            return SimpleNamespace(evidence=evidence, step=step, diagnostics=())

        def stop(self, orchestration_id: str):
            return SimpleNamespace()

    class FakeVerificationEvidenceRepository:
        def __init__(self, db) -> None:
            self.db = db

        async def record_signal_verification_evidence(self, **kwargs):
            return None

        async def upsert_signal_verification_evidence_set(self, **kwargs):
            return None

    monkeypatch.setattr(signal_test_run_runner.RedisManager, "get_instance", lambda: FakeRedis())
    monkeypatch.setattr(signal_test_run_runner.WsEventPublisher, "publish", publish_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_do_command", enqueue_do_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_request_state", enqueue_state_noop)
    monkeypatch.setattr(signal_test_run_runner, "build_verification_runtime_start_context", build_context_noop)
    monkeypatch.setattr(signal_test_run_runner, "VerificationRuntimeOrchestrator", FakeVerificationRuntimeOrchestrator)
    monkeypatch.setattr(signal_test_run_runner, "VerificationEvidenceRepository", FakeVerificationEvidenceRepository)

    job_state = {
        "job_id": "job-1",
        "workspace_id": 7,
        "operation": "test_run",
        "status": "queued",
        "created_at": "2026-01-01T12:30:00+00:00",
    }
    payload = {
        "job_id": "job-1",
        "signal_ids": [1, 2],
        "signal_interval_ms": 100,
        "toggle_mode": "single",
        "verification_enabled": True,
        "verification_runtime_version": "mms",
        "verification_signal_list_revision_id": 2,
    }

    result = run_async(signal_test_run_runner._handle_test_run(MixedRowsRepo(), 7, payload, job_state))  # type: ignore[arg-type]

    assert context_signal_ids == [[1]]
    assert captured_signal_ids == [1]
    assert result["succeeded"] == 2
    assert result["verification_observed"] == 1
    assert result["verification_requested_signal_count"] == 1
    assert [item[0] for item in commands].count("do") == 2


def test_signal_test_run_continues_commands_when_iec61850_preparation_is_not_ready(monkeypatch) -> None:
    commands: list[tuple[str, dict]] = []

    class MappedRowsRepo(FakeLiveRowsRepo):
        async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids: list[int]):
            self.calls.append([int(signal_id) for signal_id in signal_ids])
            return [
                build_allocation_row(
                    int(signal_id),
                    signal_metadata={
                        "verification": {
                            "enabled": True,
                            "transport_host": "172.16.40.128:12447",
                            "iec61850_address": "IEDLD0/GGIO1.stVal[ST]",
                        }
                    },
                )
                for signal_id in signal_ids
            ]

    async def publish_noop(event) -> None:
        return None

    async def enqueue_do_noop(**kwargs) -> None:
        commands.append(("do", dict(kwargs)))

    async def enqueue_state_noop(**kwargs) -> None:
        commands.append(("state", dict(kwargs)))

    async def build_context_failed(**kwargs):
        raise ValueError("IEC 61850 verification plan is not ready for selected signal_id values: [1]")

    monkeypatch.setattr(signal_test_run_runner.RedisManager, "get_instance", lambda: FakeRedis())
    monkeypatch.setattr(signal_test_run_runner.WsEventPublisher, "publish", publish_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_do_command", enqueue_do_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_request_state", enqueue_state_noop)
    monkeypatch.setattr(signal_test_run_runner, "build_verification_runtime_start_context", build_context_failed)

    job_state = {
        "job_id": "job-1",
        "workspace_id": 7,
        "operation": "test_run",
        "status": "queued",
        "created_at": "2026-01-01T12:30:00+00:00",
    }
    payload = {
        "job_id": "job-1",
        "signal_ids": [1],
        "signal_interval_ms": 100,
        "toggle_mode": "single",
        "verification_enabled": True,
        "verification_runtime_version": "mms",
        "verification_signal_list_revision_id": 2,
    }

    result = run_async(signal_test_run_runner._handle_test_run(MappedRowsRepo(), 7, payload, job_state))  # type: ignore[arg-type]

    assert result["succeeded"] == 1
    assert result["verification_available"] is False
    assert result["verification_requested_signal_count"] == 1
    assert result["verification_observed"] == 0
    assert result["verification_prepare_error"] == "IEC 61850 verification plan is not ready for selected signal_id values: [1]"
    assert [item[0] for item in commands[:2]] == ["state", "do"]
    assert commands[0][1]["mode"].name == "REQ_ALL_BIT"


def test_signal_test_run_skips_offline_peripheral_rows_and_runs_online_selection(monkeypatch) -> None:
    commands: list[tuple[str, dict]] = []
    published_events: list[dict] = []
    context_signal_ids: list[list[int]] = []

    class MappedRowsRepo(FakeLiveRowsRepo):
        async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids: list[int]):
            self.calls.append([int(signal_id) for signal_id in signal_ids])
            return [
                build_allocation_row(
                    int(signal_id),
                    unit_online=int(signal_id) != 2,
                    signal_metadata={
                        "verification": {
                            "enabled": True,
                            "transport_host": "172.16.40.128:12447",
                            "iec61850_address": "IEDLD0/GGIO1.stVal[ST]",
                        }
                    },
                )
                for signal_id in signal_ids
            ]

    async def publish_capture(event) -> None:
        published_events.append(event.model_dump(mode="json") if hasattr(event, "model_dump") else dict(event))

    async def enqueue_do_noop(**kwargs) -> None:
        commands.append(("do", dict(kwargs)))

    async def enqueue_state_noop(**kwargs) -> None:
        commands.append(("state", dict(kwargs)))

    async def build_context_failed(**kwargs):
        context_signal_ids.append(list(kwargs["payload"].signal_ids))
        raise ValueError("IEC 61850 verification plan is not ready for selected signal_id values: [1]")

    monkeypatch.setattr(signal_test_run_runner.RedisManager, "get_instance", lambda: FakeRedis())
    monkeypatch.setattr(signal_test_run_runner.WsEventPublisher, "publish", publish_capture)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_do_command", enqueue_do_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_request_state", enqueue_state_noop)
    monkeypatch.setattr(signal_test_run_runner, "build_verification_runtime_start_context", build_context_failed)

    job_state = {
        "job_id": "job-1",
        "workspace_id": 7,
        "operation": "test_run",
        "status": "queued",
        "created_at": "2026-01-01T12:30:00+00:00",
    }
    payload = {
        "job_id": "job-1",
        "signal_ids": [1, 2],
        "signal_interval_ms": 100,
        "toggle_mode": "single",
        "verification_enabled": True,
        "verification_runtime_version": "mms",
        "verification_signal_list_revision_id": 2,
    }

    result = run_async(signal_test_run_runner._handle_test_run(MappedRowsRepo(), 7, payload, job_state))  # type: ignore[arg-type]

    prepare_results = [
        event.get("result") for event in published_events
        if isinstance(event.get("result"), dict) and event["result"].get("phase") == "preparing_iec61850"
    ]
    assert result["processed"] == 2
    assert result["succeeded"] == 1
    assert result["skipped"] == 1
    assert result["skip_reasons"]["offline_unit"] == 1
    assert result["verification_available"] is False
    assert "peripheral device offline" in result["verification_prepare_warning"]
    assert context_signal_ids == [[1]]
    assert [item[0] for item in commands] == ["state", "do", "state", "state"]
    assert commands[0][1]["mode"].name == "REQ_ALL_BIT"
    assert prepare_results
    assert [step["id"] for step in prepare_results[-1]["verification_prepare_steps"]] == [
        "peripheral_online",
        "subscribe_reports",
        "general_interrogation",
        "start_test",
    ]
    assert prepare_results[-1]["verification_prepare_steps"][0]["status"] == "warning"
    assert prepare_results[-1]["verification_prepare_steps"][1]["status"] == "failed"


def test_mms_subscription_plan_blockers_reject_fallback_groups() -> None:
    context = SimpleNamespace(
        subscription_plan=SimpleNamespace(
            groups=[
                SimpleNamespace(
                    group_id="group-1",
                    source_classification="fallback",
                    report_control_name=None,
                    report_control_reference=None,
                    data_set_reference=None,
                )
            ]
        )
    )

    blockers = signal_test_run_runner._mms_subscription_plan_blockers(context)  # noqa: SLF001

    assert blockers == ["group-1: missing fallback planning, report control, dataset"]


def test_signal_test_run_skips_non_executable_current_bindings(monkeypatch) -> None:
    rows_by_signal_id = {
        1: build_allocation_row(1, unit_id=None),
        2: build_allocation_row(2, channel_type="di"),
        3: build_allocation_row(3, unit_online=False),
        4: build_allocation_row(4, channel_type="ao"),
    }
    commands: list[dict] = []

    class FakeRowsRepo:
        def __init__(self) -> None:
            self.db = FakeRepoDb()
            self.evidence: list[dict] = []

        async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids: list[int]):
            signal_id = int(signal_ids[0])
            return [rows_by_signal_id[signal_id]]

        async def record_signal_test_run_step_evidence(self, **kwargs):
            self.evidence.append(dict(kwargs))

    async def publish_noop(event) -> None:
        return None

    async def enqueue_do_noop(**kwargs) -> None:
        commands.append(dict(kwargs))

    monkeypatch.setattr(signal_test_run_runner.RedisManager, "get_instance", lambda: FakeRedis())
    monkeypatch.setattr(signal_test_run_runner.WsEventPublisher, "publish", publish_noop)
    monkeypatch.setattr(signal_test_run_runner, "enqueue_do_command", enqueue_do_noop)

    job_state = {
        "job_id": "job-1",
        "workspace_id": 7,
        "operation": "test_run",
        "status": "queued",
        "created_at": "2026-01-01T12:30:00+00:00",
    }
    payload = {
        "job_id": "job-1",
        "signal_ids": [1, 2, 3, 4],
        "signal_interval_ms": 100,
        "toggle_mode": "single",
    }

    repo = FakeRowsRepo()
    result = run_async(signal_test_run_runner._handle_test_run(repo, 7, payload, job_state))  # type: ignore[arg-type]

    assert result["succeeded"] == 0
    assert result["skipped"] == 4
    assert result["evidence_count"] == 4
    assert result["skip_reasons"]["invalid_binding"] == 1
    assert result["skip_reasons"]["incompatible_channel_mode"] == 1
    assert result["skip_reasons"]["offline_unit"] == 1
    assert result["skip_reasons"]["ao_profile_required"] == 1
    assert [item["reason"] for item in repo.evidence] == [
        "invalid_binding",
        "incompatible_channel_mode",
        "offline_unit",
        "ao_profile_required",
    ]
    assert repo.evidence[1]["channel_id"] == 102
    assert repo.evidence[2]["unit_id"] == "unit-3"
    assert commands == []
