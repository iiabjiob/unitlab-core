from __future__ import annotations

import hashlib
import json
from datetime import UTC, datetime
from collections.abc import Sequence

from sqlalchemy import delete, func, select, update
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.signal_revision import SignalListRevision, SignalListRevisionItem, SignalTestRunPlan, SignalTestRunPlanItem
from app.schemas.signal_sheet_schema import SignalAllocationRowSchema


def _canonical_json(value: object) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


class SignalRevisionService:
    def __init__(self, db: AsyncSession):
        self.db: AsyncSession = db

    async def create_active_revision(
        self,
        *,
        workspace_id: int,
        rows: Sequence[SignalAllocationRowSchema],
        created_by: str | None = None,
        source_hash: str | None = None,
    ) -> SignalListRevision:
        ordered_rows = list(rows)
        snapshots = [row.model_dump(mode="json") for row in ordered_rows]
        _ = await self.db.execute(
            update(SignalListRevision)
            .where(
                SignalListRevision.workspace_id == workspace_id,
                SignalListRevision.status == "active",
            )
            .values(status="archived")
        )
        content_hash = hashlib.sha256(_canonical_json(snapshots).encode("utf-8")).hexdigest()
        current_no = await self.db.scalar(
            select(func.max(SignalListRevision.revision_no)).where(SignalListRevision.workspace_id == workspace_id)
        )
        revision = SignalListRevision(
            workspace_id=workspace_id,
            revision_no=int(current_no or 0) + 1,
            status="active",
            source_hash=source_hash,
            rows_count=len(ordered_rows),
            content_hash=content_hash,
            created_by=created_by,
            activated_at=datetime.now(UTC),
        )
        revision.items = [
            SignalListRevisionItem(
                live_signal_id=row.signal_id,
                order_index=index,
                signal_key=row.signal_key,
                snapshot=snapshot,
            )
            for index, (row, snapshot) in enumerate(zip(ordered_rows, snapshots, strict=True))
        ]
        self.db.add(revision)
        await self.db.flush()
        return revision

    async def create_test_run_plan(
        self,
        *,
        job_id: str,
        workspace_id: int,
        revision_id: int,
        signal_ids: Sequence[int],
    ) -> SignalTestRunPlan:
        revision = await self.db.scalar(
            select(SignalListRevision).where(
                SignalListRevision.id == revision_id,
                SignalListRevision.workspace_id == workspace_id,
                SignalListRevision.status == "active",
            )
        )
        if revision is None:
            raise ValueError("Active signal-list revision not found")

        requested = {int(signal_id) for signal_id in signal_ids if int(signal_id) > 0}
        items = [item for item in sorted(revision.items, key=lambda item: item.order_index) if not requested or item.live_signal_id in requested]
        if requested and {item.live_signal_id for item in items} != requested:
            raise ValueError("Signal selection is not contained in the requested revision")
        snapshots = [dict(item.snapshot) for item in items]
        content_hash = hashlib.sha256(_canonical_json(snapshots).encode("utf-8")).hexdigest()
        plan = SignalTestRunPlan(
            job_id=str(job_id),
            workspace_id=workspace_id,
            revision_id=revision_id,
            content_hash=content_hash,
        )
        plan.items = [
            SignalTestRunPlanItem(
                revision_item_id=item.id,
                order_index=index,
                snapshot=snapshot,
            )
            for index, (item, snapshot) in enumerate(zip(items, snapshots, strict=True))
        ]
        self.db.add(plan)
        await self.db.flush()
        return plan

    async def get_test_run_plan(self, *, job_id: str, workspace_id: int) -> SignalTestRunPlan | None:
        return await self.db.scalar(
            select(SignalTestRunPlan).where(
                SignalTestRunPlan.job_id == str(job_id),
                SignalTestRunPlan.workspace_id == workspace_id,
            )
        )

    async def delete_test_run_plan(self, *, job_id: str, workspace_id: int) -> None:
        _ = await self.db.execute(
            delete(SignalTestRunPlan).where(
                SignalTestRunPlan.job_id == str(job_id),
                SignalTestRunPlan.workspace_id == workspace_id,
            )
        )
        await self.db.commit()

    async def clone_test_run_plan(
        self,
        *,
        source_job_id: str,
        job_id: str,
        workspace_id: int,
    ) -> SignalTestRunPlan:
        source = await self.get_test_run_plan(job_id=source_job_id, workspace_id=workspace_id)
        if source is None:
            raise ValueError("Source test-run plan is missing")
        plan = SignalTestRunPlan(
            job_id=str(job_id),
            workspace_id=workspace_id,
            revision_id=source.revision_id,
            content_hash=source.content_hash,
        )
        plan.items = [
            SignalTestRunPlanItem(
                revision_item_id=item.revision_item_id,
                order_index=item.order_index,
                snapshot=dict(item.snapshot),
            )
            for item in sorted(source.items, key=lambda item: item.order_index)
        ]
        self.db.add(plan)
        await self.db.flush()
        return plan
