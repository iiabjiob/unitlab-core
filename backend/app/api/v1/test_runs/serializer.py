from __future__ import annotations

from app.models.test_run import TestRun
from app.schemas.test_run_schema import (
    AllocationEntrySchema,
    AllocationSchema,
    TestRunRecordSchema,
    TestRunSignalSnapshotEntrySchema,
    TestRunSignalSnapshotSchema,
    TestRunSignalSnapshotSummarySchema,
)


def to_test_run_record(run: TestRun) -> TestRunRecordSchema:
    allocation_schema = None
    if run.allocation:
        allocation_entries = [
            AllocationEntrySchema(
                id=entry.id,
                channel_id=entry.channel_id,
                signal_id=entry.signal_id,
                signal_key=entry.signal.key if entry.signal else None,
                signal_metadata=entry.signal_metadata,
            )
            for entry in run.allocation.entries
        ]
        allocation_schema = AllocationSchema(
            id=run.allocation.id,
            test_run_id=run.id,
            notes=run.allocation.notes,
            created_at=run.allocation.created_at,
            updated_at=run.allocation.updated_at,
            entries=allocation_entries,
        )

    snapshot_schema = None
    if run.snapshot:
        snapshot_schema = TestRunSignalSnapshotSummarySchema(
            test_run_id=run.snapshot.test_run_id,
            workspace_id=run.snapshot.workspace_id,
            captured_at=run.snapshot.captured_at,
        )

    return TestRunRecordSchema(
        id=run.id,
        workspace_id=run.workspace_id,
        source_test_run_id=run.source_test_run_id,
        allocation_revision=run.allocation_revision,
        allocation=allocation_schema,
        sequence_ids=[link.sequence_id for link in sorted(run.sequence_links, key=lambda item: item.order_index)],
        status=run.status.value if hasattr(run.status, "value") else str(run.status),
        created_at=run.created_at,
        started_at=run.started_at,
        finished_at=run.finished_at,
        execution_meta=run.execution_meta,
        snapshot=snapshot_schema,
    )


def to_test_run_signal_snapshot(run_id: int, snapshot) -> TestRunSignalSnapshotSchema:
    entries = [
        TestRunSignalSnapshotEntrySchema(
            id=entry.id,
            snapshot_id=entry.snapshot_id,
            live_signal_id=entry.live_signal_id,
            signal_key=entry.signal_key,
            name=entry.name,
            io_direction=entry.io_direction if isinstance(entry.io_direction, str) else entry.io_direction.value,
            allocation_channel_id=entry.allocation_channel_id,
            allocation_metadata=entry.allocation_metadata,
            entry_metadata=entry.entry_metadata,
            created_at=entry.created_at,
        )
        for entry in snapshot.entries
    ]

    return TestRunSignalSnapshotSchema(
        test_run_id=run_id,
        workspace_id=snapshot.workspace_id,
        captured_at=snapshot.captured_at,
        entries=entries,
    )
