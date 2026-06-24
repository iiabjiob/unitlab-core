from __future__ import annotations

from typing import Any

from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.verification_run import SignalVerificationRun


class VerificationRunRepository:
    def __init__(self, db: AsyncSession):
        self.db = db

    async def get_signal_verification_run(
        self,
        *,
        workspace_id: int,
        test_run_id: str,
    ) -> SignalVerificationRun | None:
        stmt = select(SignalVerificationRun).where(
            SignalVerificationRun.workspace_id == workspace_id,
            SignalVerificationRun.test_run_id == test_run_id,
        )
        result = await self.db.execute(stmt.limit(1))
        return result.scalar_one_or_none()

    async def upsert_signal_verification_run(
        self,
        *,
        workspace_id: int,
        test_run_id: str,
        payload: dict[str, Any],
    ) -> SignalVerificationRun:
        run = await self.get_signal_verification_run(
            workspace_id=workspace_id,
            test_run_id=test_run_id,
        )
        if run is None:
            run = SignalVerificationRun(
                workspace_id=int(workspace_id),
                test_run_id=str(test_run_id),
                payload=dict(payload),
            )
            self.db.add(run)
        else:
            run.payload = dict(payload)

        await self.db.flush()
        return run
