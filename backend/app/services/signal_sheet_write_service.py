from __future__ import annotations

import hashlib
import time
from typing import Any, Sequence

from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.signal_sheet import SignalSheetAutoAllocateResult, SignalSheetRepository
from app.api.v1.signals import SignalsRepository
from app.core.logger import get_logger
from app.schemas.signal_import_schema import SignalImportMetaSchema
from app.schemas.signal_sheet_schema import SignalAllocationPreviewResponseSchema

logger = get_logger("service.signal_sheet_write")


class SignalSheetWriteService:
    """Application-service layer for signal sheet write flows.

    Owns transaction boundaries for synchronous HTTP mutation paths.
    """

    def __init__(
        self,
        *,
        db: AsyncSession,
        repo: SignalSheetRepository,
        signals_repo: SignalsRepository | None = None,
    ) -> None:
        self.db = db
        self.repo = repo
        self.signals_repo = signals_repo

    async def import_sheet_from_parsed_payload(
        self,
        *,
        workspace_id: int,
        raw_file_bytes: bytes,
        source_filename: str | None,
        rows_count: int,
        parsed_data: dict[str, Any],
        parsed_signals: Sequence[Any],
        import_meta: SignalImportMetaSchema | None,
        save_preset_name: str | None,
    ) -> None:
        if self.signals_repo is None:
            raise RuntimeError("SignalsRepository is required for import flow")

        try:
            await self.signals_repo.replace_from_import(workspace_id, parsed_signals)
            await self.repo.clear_allocations(workspace_id)

            source_hash = hashlib.sha256(raw_file_bytes).hexdigest()
            await self.repo.upsert_sheet(
                workspace_id=workspace_id,
                source_filename=source_filename,
                source_hash=source_hash,
                rows_count=rows_count,
                schema_version=2,
                data=parsed_data,
                import_meta=import_meta.model_dump() if import_meta else None,
            )

            if save_preset_name and import_meta is not None:
                await self.repo.save_preset(
                    workspace_id=workspace_id,
                    name=save_preset_name,
                    import_meta=import_meta,
                    commit=False,
                )

            await self.db.commit()
        except Exception:
            await self.db.rollback()
            raise

    async def save_preset(
        self,
        *,
        workspace_id: int,
        name: str,
        import_meta: SignalImportMetaSchema,
    ):
        try:
            preset = await self.repo.save_preset(
                workspace_id=workspace_id,
                name=name,
                import_meta=import_meta,
                commit=False,
            )
            await self.db.commit()
            return preset
        except Exception:
            await self.db.rollback()
            raise

    async def delete_preset(self, preset_id: int) -> bool:
        try:
            deleted = await self.repo.delete_preset(preset_id, commit=False)
            if not deleted:
                await self.db.rollback()
                return False
            await self.db.commit()
            return True
        except Exception:
            await self.db.rollback()
            raise

    async def update_allocations(self, workspace_id: int, entries: Sequence[dict[str, Any]]) -> None:
        started_at = time.monotonic()
        entry_count = len(entries)
        try:
            await self.repo.update_allocations(workspace_id, entries, commit=False)
            await self.db.commit()
            duration_ms = (time.monotonic() - started_at) * 1000
            logger.info(
                "✅ Signal allocations updated sync | workspace=%s op=bulk_update mode=sync entries=%s duration=%.1f ms",
                workspace_id,
                entry_count,
                duration_ms,
            )
        except Exception:
            await self.db.rollback()
            duration_ms = (time.monotonic() - started_at) * 1000
            logger.exception(
                "💥 Signal allocations update failed sync | workspace=%s op=bulk_update mode=sync entries=%s duration=%.1f ms",
                workspace_id,
                entry_count,
                duration_ms,
            )
            raise

    async def preview_allocation_updates(
        self,
        workspace_id: int,
        entries: Sequence[dict[str, Any]],
    ) -> SignalAllocationPreviewResponseSchema:
        return await self.repo.preview_allocation_updates(workspace_id, entries)

    async def assign_allocation(
        self,
        *,
        workspace_id: int,
        signal_id: int,
        channel_id: int,
        allocation_meta: dict[str, Any] | None = None,
    ) -> list[int]:
        started_at = time.monotonic()
        try:
            existing = await self.repo.get_allocation_by_signal_id(workspace_id, signal_id)
            if existing is not None:
                if int(existing.channel_id) == int(channel_id):
                    await self.db.commit()
                    return [signal_id]
                raise ValueError(f"Signal #{signal_id} is already allocated; use reassign")

            await self.repo.update_allocations(
                workspace_id,
                [{
                    "signal_id": int(signal_id),
                    "channel_id": int(channel_id),
                    "allocation_meta": allocation_meta,
                }],
                commit=False,
            )
            await self.db.commit()
            duration_ms = (time.monotonic() - started_at) * 1000
            logger.info(
                "✅ Signal allocation assigned | workspace=%s signal=%s channel=%s duration=%.1f ms",
                workspace_id,
                signal_id,
                channel_id,
                duration_ms,
            )
            return [signal_id]
        except Exception:
            await self.db.rollback()
            raise

    async def reassign_allocation(
        self,
        *,
        workspace_id: int,
        signal_id: int,
        channel_id: int,
        allocation_meta: dict[str, Any] | None = None,
    ) -> list[int]:
        started_at = time.monotonic()
        try:
            existing = await self.repo.get_allocation_by_signal_id(workspace_id, signal_id)
            if existing is None:
                raise ValueError(f"Signal #{signal_id} is not allocated; use assign")
            if int(existing.channel_id) == int(channel_id):
                await self.db.commit()
                return [signal_id]

            await self.repo.update_allocations(
                workspace_id,
                [{
                    "signal_id": int(signal_id),
                    "channel_id": int(channel_id),
                    "allocation_meta": allocation_meta,
                }],
                commit=False,
            )
            await self.db.commit()
            duration_ms = (time.monotonic() - started_at) * 1000
            logger.info(
                "✅ Signal allocation reassigned | workspace=%s signal=%s channel=%s duration=%.1f ms",
                workspace_id,
                signal_id,
                channel_id,
                duration_ms,
            )
            return [signal_id]
        except Exception:
            await self.db.rollback()
            raise

    async def unassign_allocation(
        self,
        *,
        workspace_id: int,
        signal_id: int,
    ) -> list[int]:
        started_at = time.monotonic()
        try:
            await self.repo.update_allocations(
                workspace_id,
                [{
                    "signal_id": int(signal_id),
                    "channel_id": None,
                }],
                commit=False,
            )
            await self.db.commit()
            duration_ms = (time.monotonic() - started_at) * 1000
            logger.info(
                "✅ Signal allocation unassigned | workspace=%s signal=%s duration=%.1f ms",
                workspace_id,
                signal_id,
                duration_ms,
            )
            return [signal_id]
        except Exception:
            await self.db.rollback()
            raise

    async def swap_allocations(
        self,
        *,
        workspace_id: int,
        signal_id: int,
        channel_id: int,
    ) -> list[int]:
        started_at = time.monotonic()
        try:
            changed_signal_ids = await self.repo.swap_allocations(
                workspace_id=workspace_id,
                signal_id=signal_id,
                channel_id=channel_id,
                commit=False,
            )
            await self.db.commit()
            duration_ms = (time.monotonic() - started_at) * 1000
            logger.info(
                "✅ Signal allocations swapped | workspace=%s signal=%s channel=%s changed=%s duration=%.1f ms",
                workspace_id,
                signal_id,
                channel_id,
                len(changed_signal_ids),
                duration_ms,
            )
            return changed_signal_ids
        except Exception:
            await self.db.rollback()
            raise

    async def auto_allocate(
        self,
        *,
        workspace_id: int,
        signal_ids: Sequence[int] | None,
        prefer_online: bool,
        prefer_single_unit: bool,
        overwrite_existing: bool,
    ) -> SignalSheetAutoAllocateResult:
        started_at = time.monotonic()
        requested_count = len(signal_ids or [])
        try:
            result = await self.repo.auto_allocate(
                workspace_id=workspace_id,
                signal_ids=signal_ids,
                prefer_online=prefer_online,
                prefer_single_unit=prefer_single_unit,
                overwrite_existing=overwrite_existing,
                commit=False,
            )
            await self.db.commit()
            duration_ms = (time.monotonic() - started_at) * 1000
            logger.info(
                "✅ Signal auto-allocate sync complete | workspace=%s op=auto_allocate mode=sync requested=%s assigned=%s skipped=%s missing=%s changed=%s duration=%.1f ms",
                workspace_id,
                requested_count,
                result.assigned,
                result.skipped,
                result.missing,
                len(result.changed_signal_ids),
                duration_ms,
            )
            return result
        except Exception:
            await self.db.rollback()
            duration_ms = (time.monotonic() - started_at) * 1000
            logger.exception(
                "💥 Signal auto-allocate sync failed | workspace=%s op=auto_allocate mode=sync requested=%s duration=%.1f ms",
                workspace_id,
                requested_count,
                duration_ms,
            )
            raise

    async def preview_auto_allocate(
        self,
        *,
        workspace_id: int,
        signal_ids: Sequence[int] | None,
        prefer_online: bool,
        prefer_single_unit: bool,
        overwrite_existing: bool,
    ) -> SignalAllocationPreviewResponseSchema:
        return await self.repo.preview_auto_allocate(
            workspace_id=workspace_id,
            signal_ids=signal_ids,
            prefer_online=prefer_online,
            prefer_single_unit=prefer_single_unit,
            overwrite_existing=overwrite_existing,
        )

    async def mark_signals_tested(self, workspace_id: int, signal_ids: Sequence[int]) -> list[int]:
        try:
            touched = await self.repo.mark_signals_tested(workspace_id, signal_ids, commit=False)
            await self.db.commit()
            return touched
        except Exception:
            await self.db.rollback()
            raise
