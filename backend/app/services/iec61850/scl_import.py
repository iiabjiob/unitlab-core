from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Protocol

from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.config import REPO_ROOT, get_settings
from app.models.workspace import Workspace
from app.models.workspace_iec61850 import (
    WorkspaceIec61850RuntimeSelection,
    WorkspaceIec61850RuntimeSelectionEvent,
    WorkspaceIec61850SclImport,
)


SCL_NORMALIZED_SCHEMA = "unitlab.iec61850.scl.normalized.v1"


class Iec61850SclImportError(RuntimeError):
    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code
        self.message = message


@dataclass(frozen=True)
class Iec61850SclCompilerDiagnostic:
    severity: str
    code: str
    message: str
    ied_name: str = ""
    access_point_name: str = ""
    logical_device_inst: str = ""
    logical_node_name: str = ""
    data_set_name: str = ""
    report_control_name: str = ""
    member_reference: str = ""


@dataclass(frozen=True)
class Iec61850SclCompilerOutput:
    schema: str
    selected_ied: str
    source_size: int
    model: dict[str, Any]
    diagnostics: tuple[Iec61850SclCompilerDiagnostic, ...]


@dataclass(frozen=True)
class Iec61850RuntimeSelectionRecord:
    selection_id: str
    workspace_id: int
    import_id: str
    runtime_revision: int
    selected_ied: str
    source_hash: str
    normalized_schema: str
    selected_by: str | None
    selection_reason: str | None


@dataclass(frozen=True)
class Iec61850SclImportRecord:
    import_id: str
    workspace_id: int
    source_filename: str | None
    source_hash: str
    source_size: int
    selected_ied: str
    normalized_schema: str
    normalized_model: dict[str, Any]
    diagnostics: tuple[Iec61850SclCompilerDiagnostic, ...]


class Iec61850SclCompiler(Protocol):
    def compile(self, source: bytes, *, selected_ied: str | None) -> Iec61850SclCompilerOutput: ...


class Iec61850SclImportRepository(Protocol):
    def save(self, record: Iec61850SclImportRecord, *, source: bytes) -> Iec61850SclImportRecord: ...


class Iec61850AsyncSclImportRepository(Protocol):
    async def save(self, record: Iec61850SclImportRecord, *, source: bytes) -> Iec61850SclImportRecord: ...


class Iec61850InMemorySclImportRepository:
    def __init__(self) -> None:
        self._records: list[tuple[Iec61850SclImportRecord, bytes]] = []

    def save(self, record: Iec61850SclImportRecord, *, source: bytes) -> Iec61850SclImportRecord:
        self._records.append((record, bytes(source)))
        return record

    def records(self) -> tuple[Iec61850SclImportRecord, ...]:
        return tuple(record for record, _source in self._records)

    def source_for(self, import_id: str) -> bytes:
        for record, source in self._records:
            if record.import_id == import_id:
                return source
        raise KeyError(import_id)


class Iec61850SqlAlchemySclImportRepository:
    def __init__(self, db: AsyncSession) -> None:
        self.db = db

    async def ensure_workspace(self, workspace_id: int) -> bool:
        result = await self.db.execute(select(Workspace.id).where(Workspace.id == workspace_id).limit(1))
        return result.scalar_one_or_none() is not None

    async def save(self, record: Iec61850SclImportRecord, *, source: bytes) -> Iec61850SclImportRecord:
        result = await self.db.execute(
            select(WorkspaceIec61850SclImport).where(
                WorkspaceIec61850SclImport.workspace_id == record.workspace_id,
                WorkspaceIec61850SclImport.source_hash == record.source_hash,
                WorkspaceIec61850SclImport.selected_ied == record.selected_ied,
            ).limit(1)
        )
        row = result.scalar_one_or_none()
        diagnostics = [_diagnostic_to_payload(item) for item in record.diagnostics]
        if row is None:
            row = WorkspaceIec61850SclImport(
                workspace_id=record.workspace_id,
                source_filename=record.source_filename,
                source_hash=record.source_hash,
                source_size=record.source_size,
                selected_ied=record.selected_ied,
                normalized_schema=record.normalized_schema,
                source_bytes=source,
                normalized_model=record.normalized_model,
                diagnostics=diagnostics,
            )
            self.db.add(row)
            await self.db.flush()
        else:
            row.source_filename = record.source_filename
            row.source_size = record.source_size
            row.normalized_schema = record.normalized_schema
            row.source_bytes = source
            row.normalized_model = record.normalized_model
            row.diagnostics = diagnostics
            await self.db.flush()
        await self.db.commit()
        await self.db.refresh(row)
        return _record_from_row(row)

    async def get_import(self, *, workspace_id: int, import_id: str) -> Iec61850SclImportRecord | None:
        row = await self._get_import_row(workspace_id=workspace_id, import_id=import_id)
        return _record_from_row(row) if row is not None else None

    async def get_active_runtime_selection(self, *, workspace_id: int) -> Iec61850RuntimeSelectionRecord | None:
        result = await self.db.execute(
            select(WorkspaceIec61850RuntimeSelection, WorkspaceIec61850SclImport)
            .join(WorkspaceIec61850SclImport, WorkspaceIec61850SclImport.id == WorkspaceIec61850RuntimeSelection.scl_import_id)
            .where(WorkspaceIec61850RuntimeSelection.workspace_id == workspace_id)
            .limit(1)
        )
        item = result.first()
        if item is None:
            return None
        selection, scl_import = item
        return _selection_record_from_rows(selection, scl_import)

    async def select_runtime_import(
        self,
        *,
        workspace_id: int,
        import_id: str,
        selected_by: str | None = None,
        reason: str | None = None,
    ) -> Iec61850RuntimeSelectionRecord:
        scl_import = await self._get_import_row(workspace_id=workspace_id, import_id=import_id)
        if scl_import is None:
            raise Iec61850SclImportError("SCL_IMPORT_NOT_FOUND", "SCL import was not found for this workspace.")

        result = await self.db.execute(
            select(WorkspaceIec61850RuntimeSelection)
            .where(WorkspaceIec61850RuntimeSelection.workspace_id == workspace_id)
            .limit(1)
        )
        selection = result.scalar_one_or_none()
        if selection is None:
            runtime_revision = 1
            selection = WorkspaceIec61850RuntimeSelection(
                workspace_id=workspace_id,
                scl_import_id=scl_import.id,
                runtime_revision=runtime_revision,
                selected_by=selected_by,
                selection_reason=reason,
            )
            self.db.add(selection)
            await self.db.flush()
        elif selection.scl_import_id == scl_import.id:
            runtime_revision = selection.runtime_revision
            selection.selected_by = selected_by
            selection.selection_reason = reason
            await self.db.flush()
        else:
            runtime_revision = int(selection.runtime_revision) + 1
            selection.scl_import_id = scl_import.id
            selection.runtime_revision = runtime_revision
            selection.selected_by = selected_by
            selection.selection_reason = reason
            await self.db.flush()

        event = WorkspaceIec61850RuntimeSelectionEvent(
            workspace_id=workspace_id,
            scl_import_id=scl_import.id,
            runtime_revision=runtime_revision,
            operation="select",
            selected_by=selected_by,
            selection_reason=reason,
            payload={
                "sourceHash": scl_import.source_hash,
                "selectedIed": scl_import.selected_ied,
                "normalizedSchema": scl_import.normalized_schema,
            },
        )
        self.db.add(event)
        await self.db.commit()
        await self.db.refresh(selection)
        return _selection_record_from_rows(selection, scl_import)

    async def _get_import_row(self, *, workspace_id: int, import_id: str) -> WorkspaceIec61850SclImport | None:
        try:
            parsed_import_id = int(import_id)
        except ValueError:
            return None
        result = await self.db.execute(
            select(WorkspaceIec61850SclImport)
            .where(
                WorkspaceIec61850SclImport.workspace_id == workspace_id,
                WorkspaceIec61850SclImport.id == parsed_import_id,
            )
            .limit(1)
        )
        return result.scalar_one_or_none()


def _diagnostic_to_payload(item: Iec61850SclCompilerDiagnostic) -> dict[str, str]:
    return {
        "severity": item.severity,
        "code": item.code,
        "message": item.message,
        "iedName": item.ied_name,
        "accessPointName": item.access_point_name,
        "logicalDeviceInst": item.logical_device_inst,
        "logicalNodeName": item.logical_node_name,
        "dataSetName": item.data_set_name,
        "reportControlName": item.report_control_name,
        "memberReference": item.member_reference,
    }


def _diagnostic_from_payload(item: dict[str, Any]) -> Iec61850SclCompilerDiagnostic:
    return Iec61850SclCompilerDiagnostic(
        severity=str(item.get("severity", "")),
        code=str(item.get("code", "")),
        message=str(item.get("message", "")),
        ied_name=str(item.get("iedName", "")),
        access_point_name=str(item.get("accessPointName", "")),
        logical_device_inst=str(item.get("logicalDeviceInst", "")),
        logical_node_name=str(item.get("logicalNodeName", "")),
        data_set_name=str(item.get("dataSetName", "")),
        report_control_name=str(item.get("reportControlName", "")),
        member_reference=str(item.get("memberReference", "")),
    )


def _record_from_row(row: WorkspaceIec61850SclImport) -> Iec61850SclImportRecord:
    diagnostics = row.diagnostics if isinstance(row.diagnostics, list) else []
    return Iec61850SclImportRecord(
        import_id=str(row.id),
        workspace_id=row.workspace_id,
        source_filename=row.source_filename,
        source_hash=row.source_hash,
        source_size=row.source_size,
        selected_ied=row.selected_ied,
        normalized_schema=row.normalized_schema,
        normalized_model=row.normalized_model if isinstance(row.normalized_model, dict) else {},
        diagnostics=tuple(_diagnostic_from_payload(item) for item in diagnostics if isinstance(item, dict)),
    )


def _selection_record_from_rows(
    selection: WorkspaceIec61850RuntimeSelection,
    scl_import: WorkspaceIec61850SclImport,
) -> Iec61850RuntimeSelectionRecord:
    return Iec61850RuntimeSelectionRecord(
        selection_id=str(selection.id),
        workspace_id=selection.workspace_id,
        import_id=str(scl_import.id),
        runtime_revision=selection.runtime_revision,
        selected_ied=scl_import.selected_ied,
        source_hash=scl_import.source_hash,
        normalized_schema=scl_import.normalized_schema,
        selected_by=selection.selected_by,
        selection_reason=selection.selection_reason,
    )


class Iec61850SclCliCompiler:
    def __init__(self, binary_path: str | Path, *, timeout_seconds: float = 10.0) -> None:
        self.binary_path = Path(binary_path)
        self.timeout_seconds = timeout_seconds

    def compile(self, source: bytes, *, selected_ied: str | None) -> Iec61850SclCompilerOutput:
        if not self.binary_path.exists():
            raise Iec61850SclImportError("SCL_COMPILER_UNAVAILABLE", f"SCL compiler binary not found: {self.binary_path}")

        with tempfile.NamedTemporaryFile(prefix="unitlab-scl-", suffix=".scd") as input_file:
            input_file.write(source)
            input_file.flush()
            command = [str(self.binary_path), "--input", input_file.name]
            if selected_ied:
                command.extend(["--ied", selected_ied])
            try:
                completed = subprocess.run(
                    command,
                    check=False,
                    capture_output=True,
                    timeout=self.timeout_seconds,
                )
            except subprocess.TimeoutExpired as exc:
                raise Iec61850SclImportError("SCL_COMPILER_TIMEOUT", "SCL compiler timed out.") from exc

        if completed.returncode != 0:
            stderr = completed.stderr.decode("utf-8", errors="replace").strip()
            raise Iec61850SclImportError("SCL_COMPILER_FAILED", stderr or f"SCL compiler exited with {completed.returncode}.")

        stdout = completed.stdout.decode("utf-8", errors="replace")
        return _parse_compiler_json(stdout)


class Iec61850SclImportService:
    def __init__(self, compiler: Iec61850SclCompiler, repository: Iec61850SclImportRepository) -> None:
        self._compiler = compiler
        self._repository = repository

    def prepare_import_record(
        self,
        *,
        workspace_id: int,
        source: bytes,
        filename: str | None = None,
        selected_ied: str | None = None,
    ) -> Iec61850SclImportRecord:
        if workspace_id <= 0:
            raise Iec61850SclImportError("SCL_WORKSPACE_INVALID", "workspace_id must be positive.")
        if not source:
            raise Iec61850SclImportError("SCL_SOURCE_EMPTY", "SCL source is empty.")

        source_hash = hashlib.sha256(source).hexdigest()
        compiled = self._compiler.compile(source, selected_ied=selected_ied)
        if compiled.schema != SCL_NORMALIZED_SCHEMA:
            raise Iec61850SclImportError("SCL_SCHEMA_UNSUPPORTED", f"Unsupported SCL normalized schema: {compiled.schema}")
        if compiled.source_size != len(source):
            raise Iec61850SclImportError("SCL_SOURCE_SIZE_MISMATCH", "Compiler source size does not match imported source bytes.")

        return Iec61850SclImportRecord(
            import_id=source_hash,
            workspace_id=workspace_id,
            source_filename=filename,
            source_hash=source_hash,
            source_size=len(source),
            selected_ied=compiled.selected_ied,
            normalized_schema=compiled.schema,
            normalized_model=compiled.model,
            diagnostics=compiled.diagnostics,
        )

    def import_scl(
        self,
        *,
        workspace_id: int,
        source: bytes,
        filename: str | None = None,
        selected_ied: str | None = None,
    ) -> Iec61850SclImportRecord:
        record = self.prepare_import_record(
            workspace_id=workspace_id,
            source=source,
            filename=filename,
            selected_ied=selected_ied,
        )
        return self._repository.save(record, source=source)


def create_scl_cli_compiler_from_settings() -> Iec61850SclCliCompiler:
    settings = get_settings()
    binary_path = settings.iec61850_scl_compiler_binary_path or _default_dev_scl_compiler_binary_path(settings.app_env)
    if not binary_path:
        raise Iec61850SclImportError("SCL_COMPILER_UNCONFIGURED", "iec61850_scl_compiler_binary_path is not configured.")
    return Iec61850SclCliCompiler(
        binary_path,
        timeout_seconds=settings.iec61850_scl_compiler_timeout_seconds,
    )


def _default_dev_scl_compiler_binary_path(app_env: str) -> str | None:
    if app_env.lower() in {"production", "prod"}:
        return None
    candidate = REPO_ROOT.parent / "iec61850_ied" / "build" / "unitlab-iec61850-scl-compiler-cli"
    return str(candidate) if candidate.exists() else None


def _parse_compiler_json(payload: str) -> Iec61850SclCompilerOutput:
    try:
        document = json.loads(payload)
    except json.JSONDecodeError as exc:
        raise Iec61850SclImportError("SCL_COMPILER_JSON_INVALID", "SCL compiler returned invalid JSON.") from exc

    diagnostics = tuple(
        Iec61850SclCompilerDiagnostic(
            severity=str(item.get("severity", "")),
            code=str(item.get("code", "")),
            message=str(item.get("message", "")),
            ied_name=str(item.get("iedName", "")),
            access_point_name=str(item.get("accessPointName", "")),
            logical_device_inst=str(item.get("logicalDeviceInst", "")),
            logical_node_name=str(item.get("logicalNodeName", "")),
            data_set_name=str(item.get("dataSetName", "")),
            report_control_name=str(item.get("reportControlName", "")),
            member_reference=str(item.get("memberReference", "")),
        )
        for item in document.get("diagnostics", [])
        if isinstance(item, dict)
    )
    model = document.get("model", {})
    if not isinstance(model, dict):
        raise Iec61850SclImportError("SCL_COMPILER_JSON_INVALID", "SCL compiler model payload is not an object.")

    return Iec61850SclCompilerOutput(
        schema=str(document.get("schema", "")),
        selected_ied=str(document.get("selectedIed", "")),
        source_size=int(document.get("sourceSize", 0)),
        model=model,
        diagnostics=diagnostics,
    )
