from __future__ import annotations

import hashlib
import json
from datetime import UTC, datetime
from typing import Sequence

from sqlalchemy import func, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.signal_revision import SignalListRevision, SignalListRevisionItem
from app.schemas.signal_sheet_schema import SignalAllocationRowSchema


def _canonical_json(value: object) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


class SignalRevisionService:
    def __init__(self, db: AsyncSession):
        self.db = db

    async def create_active_revision(
        self,
        *,
        workspace_id: int,
        rows: Sequence[SignalAllocationRowSchema],
        created_by: str | None = None,
        source_hash: str | None = None,
    ) -> SignalListRevision:
        ordered_rows = list(rows)
        snapshots = [row.model_dump(mode="json", exclude={"row_id"}) for row in ordered_rows]
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
