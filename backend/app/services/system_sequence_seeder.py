from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Awaitable, Callable, Iterable, Mapping

from sqlalchemy import delete, select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.core.logger import get_logger
from app.infrastructure.db.database import AsyncSessionLocal
from app.models.sequence import Sequence, SequenceStep, SequenceStepType
from app.models.workspace import Workspace, WorkspaceSequence

logger = get_logger("system_sequences")


@dataclass(frozen=True)
class DefaultSequenceStep:
    step_type: SequenceStepType
    payload: Mapping[str, Any] | None = None


@dataclass(frozen=True)
class DefaultSequenceDefinition:
    key: str
    name: str
    description: str
    steps: tuple[DefaultSequenceStep, ...]


DO_SIGNAL_KEY = "do_primary"
DI_SIGNAL_KEY = "di_feedback"
AO_SIGNAL_KEY = "ao_output"


DEFAULT_SEQUENCE_DEFINITIONS: tuple[DefaultSequenceDefinition, ...] = (
    DefaultSequenceDefinition(
        key="basic_do_test",
        name="Basic DO Test",
        description="Latch a DO line, hold for 500ms, unlatch, and wait for the next action.",
        steps=(
            DefaultSequenceStep(
                step_type=SequenceStepType.DO_LATCH,
                payload={"signal_key": DO_SIGNAL_KEY, "value": 1},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.WAIT,
                payload={"ms": 500},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.DO_LATCH,
                payload={"signal_key": DO_SIGNAL_KEY, "value": 0},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.WAIT,
                payload={"ms": 1000},
            ),
        ),
    ),
    DefaultSequenceDefinition(
        key="basic_di_readback",
        name="Basic DI Read-back Test",
        description="Toggle a DO line and provide dwell periods to verify DI feedback channels.",
        steps=(
            DefaultSequenceStep(
                step_type=SequenceStepType.DO_LATCH,
                payload={"signal_key": DO_SIGNAL_KEY, "value": 1},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.WAIT,
                payload={"ms": 250},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.WAIT,
                payload={"ms": 250, "signal_key": DI_SIGNAL_KEY, "note": "Verify DI reads HIGH"},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.DO_LATCH,
                payload={"signal_key": DO_SIGNAL_KEY, "value": 0},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.WAIT,
                payload={"ms": 250, "signal_key": DI_SIGNAL_KEY, "note": "Verify DI reads LOW"},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.WAIT,
                payload={"ms": 500},
            ),
        ),
    ),
    DefaultSequenceDefinition(
        key="basic_ao_sweep",
        name="Basic AO Sweep",
        description="Drive an AO channel to min, max, then back to min with short settling delays.",
        steps=(
            DefaultSequenceStep(
                step_type=SequenceStepType.AO_SET,
                payload={"signal_key": AO_SIGNAL_KEY, "value": 0.0},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.WAIT,
                payload={"ms": 250},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.AO_SET,
                payload={"signal_key": AO_SIGNAL_KEY, "value": 10.0},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.WAIT,
                payload={"ms": 500},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.AO_SET,
                payload={"signal_key": AO_SIGNAL_KEY, "value": 0.0},
            ),
            DefaultSequenceStep(
                step_type=SequenceStepType.WAIT,
                payload={"ms": 250},
            ),
        ),
    ),
)


async def seed_default_sequences(session: AsyncSession | None = None) -> None:
    async def _runner(db: AsyncSession) -> None:
        sequences = await _ensure_sequences(db)
        workspace_ids = await _get_workspace_ids(db)
        await _attach_sequences_to_workspaces(db, sequences.values(), workspace_ids)

    await _with_session(session, _runner)


async def ensure_default_sequences_for_workspace(
    workspace_id: int,
    session: AsyncSession | None = None,
) -> None:
    async def _runner(db: AsyncSession) -> None:
        sequences = await _ensure_sequences(db)
        await _attach_sequences_to_workspaces(db, sequences.values(), [workspace_id])

    await _with_session(session, _runner)


async def _with_session(
    session: AsyncSession | None,
    fn: Callable[[AsyncSession], Awaitable[None]],
) -> None:
    if session is not None:
        await fn(session)
        return
    async with AsyncSessionLocal() as managed:
        await fn(managed)
        await managed.commit()


async def _ensure_sequences(db: AsyncSession) -> dict[str, Sequence]:
    sequences: dict[str, Sequence] = {}
    for definition in DEFAULT_SEQUENCE_DEFINITIONS:
        sequence = await _upsert_sequence(db, definition)
        sequences[definition.key] = sequence
    return sequences


async def _upsert_sequence(db: AsyncSession, definition: DefaultSequenceDefinition) -> Sequence:
    stmt = (
        select(Sequence)
        .options(selectinload(Sequence.steps))
        .where(Sequence.system_key == definition.key)
    )
    result = await db.execute(stmt)
    sequence = result.scalar_one_or_none()
    if not sequence:
        sequence = Sequence(
            name=definition.name,
            description=definition.description,
            system_key=definition.key,
            system_provided=True,
            read_only=True,
        )
        db.add(sequence)
        await db.flush()
        await _replace_steps(db, sequence.id, definition.steps)
        logger.info("Created default sequence '%s'", definition.name)
        return sequence

    updated = False
    if sequence.name != definition.name:
        sequence.name = definition.name
        updated = True
    if sequence.description != definition.description:
        sequence.description = definition.description
        updated = True
    if not sequence.system_provided:
        sequence.system_provided = True
        updated = True
    if not sequence.read_only:
        sequence.read_only = True
        updated = True

    if updated:
        await db.flush()

    if not _steps_match(sequence.steps, definition.steps):
        await _replace_steps(db, sequence.id, definition.steps)
        logger.info("Synchronized steps for default sequence '%s'", definition.name)

    return sequence


def _steps_match(
    existing_steps: Iterable[SequenceStep],
    definition_steps: tuple[DefaultSequenceStep, ...],
) -> bool:
    existing = sorted(existing_steps, key=lambda step: step.order_index)
    if len(existing) != len(definition_steps):
        return False
    for current, target in zip(existing, definition_steps):
        if current.sequence_step_type != target.step_type:
            return False
        current_payload = current.payload or {}
        target_payload = dict(target.payload or {})
        if current_payload != target_payload:
            return False
    return True


async def _replace_steps(
    db: AsyncSession,
    sequence_id: int,
    steps: tuple[DefaultSequenceStep, ...],
) -> None:
    await db.execute(delete(SequenceStep).where(SequenceStep.sequence_id == sequence_id))
    for index, step_def in enumerate(steps):
        db.add(
            SequenceStep(
                sequence_id=sequence_id,
                order_index=index,
                sequence_step_type=step_def.step_type,
                payload=dict(step_def.payload or {}),
            )
        )
    await db.flush()


async def _get_workspace_ids(db: AsyncSession) -> list[int]:
    result = await db.execute(select(Workspace.id))
    return list(result.scalars().all())


async def _attach_sequences_to_workspaces(
    db: AsyncSession,
    sequences: Iterable[Sequence],
    workspace_ids: Iterable[int],
) -> None:
    workspace_list = list({wid for wid in workspace_ids if wid is not None})
    if not workspace_list:
        return
    for sequence in sequences:
        await _attach_sequence(db, sequence.id, workspace_list)


async def _attach_sequence(db: AsyncSession, sequence_id: int, workspace_ids: list[int]) -> None:
    if not workspace_ids:
        return
    stmt = select(WorkspaceSequence.workspace_id).where(
        WorkspaceSequence.sequence_id == sequence_id,
        WorkspaceSequence.workspace_id.in_(workspace_ids),
    )
    existing = set((await db.execute(stmt)).scalars().all())
    for workspace_id in workspace_ids:
        if workspace_id in existing:
            continue
        db.add(WorkspaceSequence(workspace_id=workspace_id, sequence_id=sequence_id))
    await db.flush()
