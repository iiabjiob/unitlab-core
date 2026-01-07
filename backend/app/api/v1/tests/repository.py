from __future__ import annotations

from typing import Any, Dict, Iterable, List, Optional

from sqlalchemy import func, select, update
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.models.channel import Channel
from app.models.test_run import TestRun, TestRunStep
from app.schemas.test_run_schema import TestRunSettings


async def _validate_channels(db: AsyncSession, channel_ids: Iterable[int]) -> None:
    ids = [cid for cid in channel_ids if cid is not None]
    if not ids:
        return
    stmt = select(Channel.id).where(Channel.id.in_(ids))
    result = await db.execute(stmt)
    found = set(result.scalars().all())
    missing = [cid for cid in ids if cid not in found]
    if missing:
        raise ValueError(f"Unknown channel ids: {missing}")


def _normalize_settings(payload: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    settings = TestRunSettings.model_validate(payload or {})
    return settings.model_dump()


class TestRunRepository:
    def __init__(self, db: AsyncSession):
        self.db = db

    def _with_steps(self):
        return select(TestRun).options(
            selectinload(TestRun.steps)
            .selectinload(TestRunStep.channel)
            .selectinload(Channel.device)
        )

    async def list(self, project_id: int) -> list[TestRun]:
        stmt = (
            self._with_steps()
            .where(TestRun.project_id == project_id)
            .order_by(TestRun.created_at.desc())
        )
        result = await self.db.execute(stmt)
        return list(result.scalars().unique().all())

    async def get(self, project_id: int, run_id: int) -> Optional[TestRun]:
        stmt = self._with_steps().where(
            TestRun.project_id == project_id,
            TestRun.id == run_id,
        )
        result = await self.db.execute(stmt)
        return result.scalars().unique().one_or_none()

    async def create(
        self,
        project_id: int,
        data: Dict[str, Any],
        channel_ids: List[int],
        settings: Optional[Dict[str, Any]] = None,
    ) -> TestRun:
        await _validate_channels(self.db, channel_ids)
        payload = {
            "project_id": project_id,
            **data,
            "settings": _normalize_settings(settings),
        }
        run = TestRun(**payload)
        self.db.add(run)
        await self.db.flush()

        for order_index, channel_id in enumerate(channel_ids):
            self.db.add(
                TestRunStep(
                    test_run_id=run.id,
                    order_index=order_index,
                    channel_id=channel_id,
                )
            )

        await self.db.commit()
        await self.db.refresh(run, attribute_names=["steps"])
        return run

    async def update(
        self,
        project_id: int,
        run_id: int,
        changes: Dict[str, Any],
    ) -> Optional[TestRun]:
        run = await self.get(project_id, run_id)
        if not run:
            return None

        settings_payload = changes.pop("settings", None)
        if settings_payload is not None:
            changes["settings"] = _normalize_settings(settings_payload)

        for key, value in changes.items():
            setattr(run, key, value)
        await self.db.commit()
        await self.db.refresh(run)
        return run

    async def delete(self, project_id: int, run_id: int) -> bool:
        run = await self.get(project_id, run_id)
        if not run:
            return False
        await self.db.delete(run)
        await self.db.commit()
        return True

    async def ensure(self, project_id: int, run_id: int) -> bool:
        stmt = select(TestRun.id).where(
            TestRun.project_id == project_id,
            TestRun.id == run_id,
        )
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none() is not None


class TestRunStepRepository:
    def __init__(self, db: AsyncSession):
        self.db = db

    def _with_channel(self):
        return select(TestRunStep).options(
            selectinload(TestRunStep.channel).selectinload(Channel.device)
        )

    async def list_for_run(self, run_id: int) -> list[TestRunStep]:
        stmt = (
            self._with_channel()
            .where(TestRunStep.test_run_id == run_id)
            .order_by(TestRunStep.order_index)
        )
        result = await self.db.execute(stmt)
        return result.scalars().all()

    async def create(self, run_id: int, channel_id: int) -> TestRunStep:
        await _validate_channels(self.db, [channel_id])
        stmt = (
            select(TestRunStep.order_index)
            .where(TestRunStep.test_run_id == run_id)
            .order_by(TestRunStep.order_index.desc())
            .limit(1)
        )
        last_index = (await self.db.execute(stmt)).scalar_one_or_none()
        next_index = 0 if last_index is None else last_index + 1
        step = TestRunStep(
            test_run_id=run_id,
            order_index=next_index,
            channel_id=channel_id,
        )
        self.db.add(step)
        await self._touch_run(run_id)
        await self.db.commit()
        await self.db.refresh(step)
        return step

    async def bulk_create(self, run_id: int, channel_ids: List[int]) -> list[TestRunStep]:
        await _validate_channels(self.db, channel_ids)
        if not channel_ids:
            return await self.list_for_run(run_id)

        stmt = (
            select(TestRunStep.order_index)
            .where(TestRunStep.test_run_id == run_id)
            .order_by(TestRunStep.order_index.desc())
            .limit(1)
        )
        last_index = (await self.db.execute(stmt)).scalar_one_or_none()
        next_index = 0 if last_index is None else last_index + 1

        for offset, channel_id in enumerate(channel_ids):
            self.db.add(
                TestRunStep(
                    test_run_id=run_id,
                    order_index=next_index + offset,
                    channel_id=channel_id,
                )
            )

        await self._touch_run(run_id)
        await self.db.commit()
        return await self.list_for_run(run_id)

    async def update(self, step_id: int, changes: Dict[str, Any]) -> Optional[TestRunStep]:
        stmt = self._with_channel().where(TestRunStep.id == step_id)
        result = await self.db.execute(stmt)
        step = result.scalar_one_or_none()
        if not step:
            return None

        channel_id = changes.get("channel_id")
        if channel_id is not None:
            await _validate_channels(self.db, [channel_id])

        for key, value in changes.items():
            if value is None:
                continue
            if key == "order_index":
                step.order_index = int(value)
            elif hasattr(step, key):
                setattr(step, key, value)

        await self._touch_run(step.test_run_id)
        await self.db.commit()
        await self.db.refresh(step)
        return step

    async def delete(self, step_id: int) -> bool:
        stmt = select(TestRunStep).where(TestRunStep.id == step_id)
        result = await self.db.execute(stmt)
        step = result.scalar_one_or_none()
        if not step:
            return False
        run_id = step.test_run_id
        await self.db.delete(step)
        await self.db.flush()
        await self._touch_run(run_id)
        await self.db.commit()
        await self.normalize(run_id)
        return True

    async def reorder(self, run_id: int, new_order: List[int]) -> list[TestRunStep]:
        steps = await self.list_for_run(run_id)
        lookup = {step.id: step for step in steps}
        resolved: list[int] = []
        seen: set[int] = set()
        for step_id in new_order:
            if step_id in lookup and step_id not in seen:
                resolved.append(step_id)
                seen.add(step_id)
        for step_id in lookup:
            if step_id not in seen:
                resolved.append(step_id)

        await self._apply_order(run_id, resolved)
        return await self.list_for_run(run_id)

    async def normalize(self, run_id: int) -> list[TestRunStep]:
        steps = await self.list_for_run(run_id)
        ordered_ids = [step.id for step in steps]
        await self._apply_order(run_id, ordered_ids)
        return await self.list_for_run(run_id)

    async def _apply_order(self, run_id: int, ordered_ids: List[int]) -> None:
        if not ordered_ids:
            return
        # offset indices to avoid unique constraint collisions
        await self.db.execute(
            update(TestRunStep)
            .where(TestRunStep.test_run_id == run_id)
            .values(order_index=TestRunStep.order_index + len(ordered_ids) + 1)
        )
        await self.db.flush()
        for index, step_id in enumerate(ordered_ids):
            await self.db.execute(
                update(TestRunStep)
                .where(TestRunStep.id == step_id)
                .values(order_index=index)
            )
        await self._touch_run(run_id)
        await self.db.commit()

    async def replace(self, run_id: int, channel_payloads: List[Dict[str, Any]]) -> list[TestRunStep]:
        await _validate_channels(self.db, [item.get("channel_id") for item in channel_payloads if item])
        existing = await self.list_for_run(run_id)
        for step in existing:
            await self.db.delete(step)
        await self.db.flush()

        for order_index, payload in enumerate(channel_payloads):
            self.db.add(
                TestRunStep(
                    test_run_id=run_id,
                    order_index=order_index,
                    channel_id=payload.get("channel_id"),
                )
            )

        await self._touch_run(run_id)
        await self.db.commit()
        return await self.list_for_run(run_id)

    async def _touch_run(self, run_id: int) -> None:
        await self.db.execute(
            update(TestRun)
            .where(TestRun.id == run_id)
            .values(updated_at=func.now())
        )
